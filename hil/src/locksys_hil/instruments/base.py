# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Abstract instrument interfaces (LS-HIL-001 section 9.1, layer 2).

Tests use these interfaces only; vendor adapters are selected from the bench configuration and
the instrument identity. Every driver implements ``safe_state`` (outputs off or released).
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class CanFrame:
    """Classic CAN data frame with an optional receive timestamp."""

    arbitration_id: int
    data: bytes
    timestamp: float | None = None


class Instrument(ABC):
    """Common behaviour of all bench instruments."""

    @abstractmethod
    def identify(self) -> str:
        """Return the instrument identity (``*IDN?`` or equivalent)."""

    @abstractmethod
    def safe_state(self) -> None:
        """Bring the instrument to its safe state; must not raise for an idle instrument."""

    def close(self) -> None:  # noqa: B027 - optional override
        """Release the transport."""


class Psu(Instrument):
    """Programmable power supply; channels are numbered from 1."""

    @abstractmethod
    def set_voltage(self, channel: int, volts: float) -> None:
        """Set the output voltage set point."""

    @abstractmethod
    def set_current_limit(self, channel: int, amps: float) -> None:
        """Set the current limit."""

    @abstractmethod
    def set_output(self, channel: int, enabled: bool) -> None:
        """Switch an output on or off."""

    @abstractmethod
    def output_enabled(self, channel: int) -> bool:
        """Return the output state read back from the instrument."""

    @abstractmethod
    def measure_voltage(self, channel: int) -> float:
        """Return the measured output voltage."""

    @abstractmethod
    def measure_current(self, channel: int) -> float:
        """Return the measured output current."""


class Dmm(Instrument):
    """Digital multimeter."""

    @abstractmethod
    def measure_dc_voltage(self) -> float:
        """Return one DC voltage reading."""


class Scope(Instrument):
    """Oscilloscope (manual evidence and analog measurements)."""

    @abstractmethod
    def arm_single(self) -> None:
        """Arm a single acquisition."""

    @abstractmethod
    def save_screenshot(self, path: Path) -> Path:
        """Save the screen image and return its path."""


class LogicAnalyzer(Instrument):
    """Logic analyser (Saleae Logic 2 automation)."""

    @abstractmethod
    def start_capture(self, profile: str, duration_s: float, out_dir: Path) -> None:
        """Start a capture with a channel profile (A-D)."""

    @abstractmethod
    def wait_capture(self) -> Path:
        """Wait for the capture to finish and return the exported data directory."""


class CanBus(Instrument):
    """USB-CAN adapter."""

    @abstractmethod
    def send(self, frame: CanFrame) -> None:
        """Transmit one frame."""

    @abstractmethod
    def recv(self, timeout_s: float) -> CanFrame | None:
        """Return the next received frame, or None after the time-out."""


class Stimulus(Instrument):
    """Stimulus MCU (SCPI-like protocol, LS-HIL-001 section 7.4)."""

    @abstractmethod
    def write(self, command: str) -> None:
        """Send a command that returns nothing."""

    @abstractmethod
    def query(self, command: str) -> str:
        """Send a query and return its single response line."""

    def sync_pulse(self, width_us: int = 10) -> int:
        """Emit a SYNC marker pulse and return its timestamp in microseconds."""
        return int(self.query(f"SYNC:PULSE? {width_us}"))

    def check_errors(self) -> None:
        """Raise when the error queue is not empty."""
        reply = self.query("SYST:ERR?")
        if not reply.startswith("0,"):
            raise RuntimeError(f"stimulus error: {reply}")
