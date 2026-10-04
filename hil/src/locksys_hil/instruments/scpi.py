# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""VISA/SCPI transport for LAN and USB instruments (PyVISA 1.16.2, pyvisa-py 0.8.1).

Vendor command sets are not fixed until the instrument models are chosen; ``ScpiPsu``
implements the common SCPI-99 subset and checks the error queue after every configuration step.
"""

from typing import Any

from locksys_hil.errors import BenchInfrastructureError
from locksys_hil.instruments.base import Psu


class ScpiTransport:
    """One VISA resource with SCPI error checking."""

    _rm: Any
    _res: Any

    def __init__(self, resource: str, backend: str = "@py", timeout_ms: int = 2000) -> None:
        """Open ``resource`` with the given VISA backend.

        Raises:
            BenchInfrastructureError: the resource cannot be opened.
        """
        import pyvisa

        try:
            self._rm = pyvisa.ResourceManager(backend)
            self._res = self._rm.open_resource(resource)
        except Exception as exc:  # pyvisa raises backend-specific types
            raise BenchInfrastructureError(f"cannot open {resource}: {exc}") from exc
        self._res.timeout = timeout_ms
        self._res.read_termination = "\n"
        self._res.write_termination = "\n"

    def write(self, command: str) -> None:
        """Send a command and check the error queue."""
        self._res.write(command)
        self.check_errors()

    def query(self, command: str) -> str:
        """Send a query and return the stripped response."""
        return str(self._res.query(command)).strip()

    def check_errors(self) -> None:
        """Raise when ``SYST:ERR?`` reports an error.

        Raises:
            BenchInfrastructureError: the instrument reported an error.
        """
        reply = self.query("SYST:ERR?")
        if not reply.startswith(("0,", "+0,")):
            raise BenchInfrastructureError(f"instrument error: {reply}")

    def close(self) -> None:
        """Close the resource."""
        self._res.close()
        self._rm.close()


class ScpiPsu(Psu):
    """Power supply using the SCPI-99 ``INST:NSEL``/``VOLT``/``CURR``/``OUTP`` subset."""

    def __init__(self, transport: ScpiTransport) -> None:
        """Wrap an open transport."""
        self._t = transport

    def _select(self, channel: int) -> None:
        self._t.write(f"INST:NSEL {channel}")

    def identify(self) -> str:
        """Return ``*IDN?``."""
        return self._t.query("*IDN?")

    def safe_state(self) -> None:
        """Switch all outputs off."""
        self._t.write("OUTP:ALL OFF")

    def set_voltage(self, channel: int, volts: float) -> None:
        """Set the voltage set point."""
        self._select(channel)
        self._t.write(f"VOLT {volts:.3f}")

    def set_current_limit(self, channel: int, amps: float) -> None:
        """Set the current limit."""
        self._select(channel)
        self._t.write(f"CURR {amps:.3f}")

    def set_output(self, channel: int, enabled: bool) -> None:
        """Switch an output."""
        self._select(channel)
        self._t.write(f"OUTP {'ON' if enabled else 'OFF'}")

    def output_enabled(self, channel: int) -> bool:
        """Read back the output state."""
        self._select(channel)
        return self._t.query("OUTP?") in {"1", "ON"}

    def measure_voltage(self, channel: int) -> float:
        """Measure the output voltage."""
        self._select(channel)
        return float(self._t.query("MEAS:VOLT?"))

    def measure_current(self, channel: int) -> float:
        """Measure the output current."""
        self._select(channel)
        return float(self._t.query("MEAS:CURR?"))

    def close(self) -> None:
        """Close the transport."""
        self._t.close()
