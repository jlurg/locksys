# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""In-memory instrument fakes for hardware-free tests and the ``fake`` bench driver."""

import queue
from dataclasses import dataclass, field
from pathlib import Path

from locksys_hil.instruments.base import (
    CanBus,
    CanFrame,
    Dmm,
    LogicAnalyzer,
    Psu,
    Scope,
    Stimulus,
)


@dataclass
class _PsuChannel:
    volts: float = 0.0
    amps_limit: float = 0.0
    enabled: bool = False


class FakePsu(Psu):
    """Power supply model: an enabled output reads back its set point and zero current."""

    def __init__(self, channels: int = 2) -> None:
        """Create a supply with ``channels`` outputs, all off."""
        self.channels = {n: _PsuChannel() for n in range(1, channels + 1)}
        self.log: list[str] = []

    def identify(self) -> str:
        """Return a fixed identity."""
        return "LOCKSYS,FAKE-PSU,0,1.0"

    def safe_state(self) -> None:
        """Switch every output off."""
        for number in self.channels:
            self.set_output(number, False)

    def set_voltage(self, channel: int, volts: float) -> None:
        """Set the voltage set point."""
        self.channels[channel].volts = volts
        self.log.append(f"VOLT {channel},{volts}")

    def set_current_limit(self, channel: int, amps: float) -> None:
        """Set the current limit."""
        self.channels[channel].amps_limit = amps
        self.log.append(f"CURR {channel},{amps}")

    def set_output(self, channel: int, enabled: bool) -> None:
        """Switch an output."""
        self.channels[channel].enabled = enabled
        self.log.append(f"OUTP {channel},{'ON' if enabled else 'OFF'}")

    def output_enabled(self, channel: int) -> bool:
        """Return the output state."""
        return self.channels[channel].enabled

    def measure_voltage(self, channel: int) -> float:
        """Return the set point of an enabled output, else 0 V."""
        state = self.channels[channel]
        return state.volts if state.enabled else 0.0

    def measure_current(self, channel: int) -> float:
        """Return 0 A (no load)."""
        return 0.0


class FakeDmm(Dmm):
    """Multimeter returning a programmable value."""

    def __init__(self, value: float = 0.0) -> None:
        """Create a meter that reads ``value``."""
        self.value = value

    def identify(self) -> str:
        """Return a fixed identity."""
        return "LOCKSYS,FAKE-DMM,0,1.0"

    def safe_state(self) -> None:
        """Nothing to release."""

    def measure_dc_voltage(self) -> float:
        """Return the programmed value."""
        return self.value


class FakeScope(Scope):
    """Oscilloscope that writes empty screenshots."""

    def identify(self) -> str:
        """Return a fixed identity."""
        return "LOCKSYS,FAKE-SCOPE,0,1.0"

    def safe_state(self) -> None:
        """Nothing to release."""

    def arm_single(self) -> None:
        """Accept the arm request."""

    def save_screenshot(self, path: Path) -> Path:
        """Create an empty file at ``path``."""
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"")
        return path


class FakeLogicAnalyzer(LogicAnalyzer):
    """Logic analyser that produces an empty export directory."""

    def __init__(self) -> None:
        """Create an idle analyser."""
        self._out: Path | None = None

    def identify(self) -> str:
        """Return a fixed identity."""
        return "LOCKSYS,FAKE-LOGIC,0,1.0"

    def safe_state(self) -> None:
        """Abandon any capture."""
        self._out = None

    def start_capture(self, profile: str, duration_s: float, out_dir: Path) -> None:
        """Record the export directory."""
        out_dir.mkdir(parents=True, exist_ok=True)
        self._out = out_dir

    def wait_capture(self) -> Path:
        """Return the export directory.

        Raises:
            RuntimeError: no capture was started.
        """
        if self._out is None:
            raise RuntimeError("no capture started")
        return self._out


class FakeCanBus(CanBus):
    """Loopback CAN bus: sent frames are recorded; ``inject`` queues frames for ``recv``."""

    def __init__(self) -> None:
        """Create an empty bus."""
        self.sent: list[CanFrame] = []
        self._rx: queue.Queue[CanFrame] = queue.Queue()

    def identify(self) -> str:
        """Return a fixed identity."""
        return "LOCKSYS,FAKE-CAN,0,1.0"

    def safe_state(self) -> None:
        """Drop pending frames."""
        while not self._rx.empty():
            self._rx.get_nowait()

    def send(self, frame: CanFrame) -> None:
        """Record a transmitted frame."""
        self.sent.append(frame)

    def inject(self, frame: CanFrame) -> None:
        """Queue a frame as if received from the bus."""
        self._rx.put(frame)

    def recv(self, timeout_s: float) -> CanFrame | None:
        """Return the next injected frame, or None."""
        try:
            return self._rx.get(timeout=timeout_s)
        except queue.Empty:
            return None


@dataclass
class FakeStimulus(Stimulus):
    """Minimal SCPI responder of the stimulus protocol 1.0."""

    version: str = "1.0"
    mode: str = "OFF"
    commands: list[str] = field(default_factory=list)
    _time_us: int = 0

    def identify(self) -> str:
        """Return a fixed identity."""
        return "LOCKSYS,HIL-STIM,FAKE0001,0.0.0"

    def safe_state(self) -> None:
        """Release all SIM outputs."""
        self.write("*RST")

    def write(self, command: str) -> None:
        """Record a command and apply ``MODE`` and ``*RST``."""
        self.commands.append(command)
        if command == "*RST":
            self.mode = "OFF"
        elif command.startswith("MODE "):
            self.mode = command.split(" ", 1)[1]

    def query(self, command: str) -> str:
        """Answer the identity, version, mode, error and SYNC queries."""
        self.commands.append(command)
        if command == "*IDN?":
            return self.identify()
        if command == "SYST:VERS?":
            return self.version
        if command == "MODE?":
            return self.mode
        if command == "SYST:ERR?":
            return '0,"No error"'
        if command.startswith("SYNC:PULSE?"):
            self._time_us += 1000
            return str(self._time_us)
        if command == "*TST?":
            return "0"
        return '-113,"Undefined header"'
