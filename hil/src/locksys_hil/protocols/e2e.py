# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""E2E protection reference implementation (LS-SAIC-001 section 7.2).

Independent Python reference of the CAN E2E profile: CRC-8/SAE-J1850 in byte 0 over
[DataID low, DataID high, bytes 1 .. DLC-1], alive counter in byte 1 bits 0-3, and the
receiver state machine for Cyclic and Event mode. It is checked against
``interfaces/vectors/e2e_v1.json`` and used by the restbus and the CAN monitors.
"""

from dataclasses import dataclass, field
from enum import Enum, StrEnum

CRC8_POLY = 0x1D
CRC8_INIT = 0xFF
CRC8_XOROUT = 0xFF
COUNTER_MASK = 0x0F
COUNTER_MODULO = 16
N_OK_VALID = 2
"""Consecutive OK frames that make a cyclic message VALID (`n_e2e_ok_valid`)."""
N_ERR_INVALID = 3
"""Consecutive errors that make a cyclic message INVALID (`n_e2e_err_invalid`)."""


def crc8_j1850(data: bytes, crc: int = CRC8_INIT, *, final: bool = True) -> int:
    """Compute CRC-8/SAE-J1850 (poly 0x1D, init 0xFF, xorout 0xFF, no reflection).

    Args:
        data: Input bytes.
        crc: Start value (``CRC8_INIT`` or an intermediate value when ``final`` was False).
        final: Apply the final XOR.

    Returns:
        The CRC value (0x4B for ASCII "123456789").
    """
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ CRC8_POLY) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc ^ CRC8_XOROUT if final else crc


def crc_input(frame: bytes | bytearray, data_id: int) -> bytes:
    """Return the CRC input of a frame: DataID low byte, DataID high byte, bytes 1 .. DLC-1."""
    return bytes((data_id & 0xFF, (data_id >> 8) & 0xFF)) + bytes(frame[1:])


def compute_crc(frame: bytes | bytearray, data_id: int) -> int:
    """Return the E2E CRC of a frame (byte 0 is ignored)."""
    return crc8_j1850(crc_input(frame, data_id))


def protect(payload: bytes | bytearray, data_id: int, counter: int) -> bytes:
    """Apply E2E protection to a payload.

    Args:
        payload: Frame bytes; byte 0 and the low nibble of byte 1 are overwritten.
        data_id: 16-bit DataID of the message.
        counter: Alive counter (taken modulo 16).

    Returns:
        The protected frame.
    """
    if len(payload) < 2:
        raise ValueError("an E2E frame has at least 2 bytes")
    frame = bytearray(payload)
    frame[1] = (frame[1] & 0xF0) | (counter & COUNTER_MASK)
    frame[0] = compute_crc(frame, data_id)
    return bytes(frame)


def counter_of(frame: bytes | bytearray) -> int:
    """Return the alive counter of a frame."""
    return frame[1] & COUNTER_MASK


class E2eMode(Enum):
    """Receiver mode of a message (`LsE2eMode`)."""

    CYCLIC = "Cyclic"
    EVENT = "Event"


class E2eStatus(StrEnum):
    """Per-frame check result."""

    OK = "OK"
    REPEATED = "REPEATED"
    WRONG_SEQUENCE = "WRONG_SEQUENCE"
    CRC_ERROR = "CRC_ERROR"


class E2eState(StrEnum):
    """Receiver state of a message."""

    INIT = "INIT"
    VALID = "VALID"
    INVALID = "INVALID"


@dataclass
class E2eCounters:
    """Error counters kept per received message (DID 0xFD08 on the DCU)."""

    crc: int = 0
    sequence: int = 0
    repeated: int = 0
    timeout: int = 0


@dataclass(frozen=True)
class E2eResult:
    """Outcome of one received frame."""

    status: E2eStatus
    state: E2eState
    delivered: bool


@dataclass
class E2eReceiver:
    """Receiver state machine of one message (LS-SAIC-001 section 7.2).

    Attributes:
        data_id: DataID of the message.
        dlc: Expected frame length.
        mode: Cyclic or Event.
        max_delta: Largest accepted counter step (`LsE2eMaxDelta`).
        n_ok_valid: Consecutive OK frames for VALID.
        n_err_invalid: Consecutive errors for INVALID.
    """

    data_id: int
    dlc: int
    mode: E2eMode = E2eMode.CYCLIC
    max_delta: int = 3
    n_ok_valid: int = N_OK_VALID
    n_err_invalid: int = N_ERR_INVALID
    counters: E2eCounters = field(default_factory=E2eCounters)
    state: E2eState = field(init=False)
    _reference: int | None = field(default=None, init=False)
    _ok_run: int = field(default=0, init=False)
    _err_run: int = field(default=0, init=False)

    def __post_init__(self) -> None:
        self.state = E2eState.VALID if self.mode is E2eMode.EVENT else E2eState.INIT

    def receive(self, frame: bytes | bytearray) -> E2eResult:
        """Check one received frame in arrival order and update the state."""
        if len(frame) != self.dlc or frame[0] != compute_crc(frame, self.data_id):
            self.counters.crc += 1
            return self._error(E2eStatus.CRC_ERROR)
        if self.mode is E2eMode.EVENT:
            return E2eResult(E2eStatus.OK, self.state, True)
        counter = counter_of(frame)
        if self._reference is None:
            self._reference = counter
            return self._ok()
        delta = (counter - self._reference) % COUNTER_MODULO
        if delta == 0:
            self.counters.repeated += 1
            return self._error(E2eStatus.REPEATED)
        self._reference = counter
        if delta > self.max_delta:
            self.counters.sequence += 1
            return self._error(E2eStatus.WRONG_SEQUENCE)
        return self._ok()

    def timeout(self) -> E2eState:
        """Signal an RX timeout: INVALID and the reference counter is cleared."""
        if self.mode is E2eMode.EVENT:
            return self.state
        self.counters.timeout += 1
        self.state = E2eState.INVALID
        self._reference = None
        self._ok_run = 0
        self._err_run = 0
        return self.state

    def _ok(self) -> E2eResult:
        self._ok_run += 1
        self._err_run = 0
        if self.state is not E2eState.VALID and self._ok_run >= self.n_ok_valid:
            self.state = E2eState.VALID
        return E2eResult(E2eStatus.OK, self.state, self.state is E2eState.VALID)

    def _error(self, status: E2eStatus) -> E2eResult:
        if self.mode is E2eMode.CYCLIC:
            self._err_run += 1
            self._ok_run = 0
            if self._err_run >= self.n_err_invalid:
                self.state = E2eState.INVALID
        return E2eResult(status, self.state, False)


@dataclass
class E2eSender:
    """Sender side: the counter advances for every frame handed to the controller."""

    data_id: int
    counter: int = 0

    def next_frame(self, payload: bytes | bytearray) -> bytes:
        """Protect ``payload`` with the current counter and advance the counter."""
        frame = protect(payload, self.data_id, self.counter)
        self.counter = (self.counter + 1) % COUNTER_MODULO
        return frame
