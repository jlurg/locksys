# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Independent Python reference of the LockSys E2E profile and APP session crypto.

Written from LS-SAIC-001 sections 7.2, 8.2 and 8.3 only (standard library); used to compute and
verify interfaces/vectors/*.json. It is not shared with the C or Dart implementations.
"""

from __future__ import annotations

import hashlib
import hmac
from dataclasses import dataclass, field

# --------------------------------------------------------------------------- CRC-8/SAE-J1850

CRC8_POLY = 0x1D
CRC8_INIT = 0xFF
CRC8_XOROUT = 0xFF


def crc8_j1850(data: bytes, crc: int = CRC8_INIT, final: bool = True) -> int:
    """CRC-8/SAE-J1850: poly 0x1D, init 0xFF, no reflection, xorout 0xFF (bitwise)."""
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ CRC8_POLY) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc ^ CRC8_XOROUT if final else crc


# --------------------------------------------------------------------------- E2E profile

STATUS_OK = "OK"
STATUS_CRC_ERROR = "CRC_ERROR"
STATUS_REPEATED = "REPEATED"
STATUS_WRONG_SEQUENCE = "WRONG_SEQUENCE"

STATE_INIT = "INIT"
STATE_VALID = "VALID"
STATE_INVALID = "INVALID"


def e2e_crc_input(data_id: int, frame: bytes) -> bytes:
    """CRC input: DataID low byte, DataID high byte, frame bytes 1 .. DLC-1."""
    return bytes((data_id & 0xFF, (data_id >> 8) & 0xFF)) + frame[1:]


def e2e_protect(data_id: int, frame: bytes, counter: int) -> bytes:
    """Write the alive counter (byte 1, bits 0-3) and the CRC (byte 0) into a frame."""
    out = bytearray(frame)
    out[1] = (out[1] & 0xF0) | (counter & 0x0F)
    out[0] = crc8_j1850(e2e_crc_input(data_id, bytes(out)))
    return bytes(out)


@dataclass
class E2eReceiver:
    """Receiver state machine of LS-SAIC-001 section 7.2 for one message."""

    data_id: int
    dlc: int
    max_delta: int
    cyclic: bool = True
    n_ok_valid: int = 2
    n_err_invalid: int = 3
    state: str = STATE_INIT
    ref: int | None = None
    ok_count: int = 0
    err_count: int = 0
    counters: dict[str, int] = field(
        default_factory=lambda: {"crc": 0, "sequence": 0, "repeated": 0, "timeout": 0}
    )

    def _error(self) -> None:
        self.err_count = min(self.err_count + 1, 255)
        self.ok_count = 0
        if self.err_count >= self.n_err_invalid:
            self.state = STATE_INVALID

    def _ok(self, counter: int) -> None:
        self.ok_count = min(self.ok_count + 1, 255)
        self.err_count = 0
        self.ref = counter
        if not self.cyclic or self.ok_count >= self.n_ok_valid:
            self.state = STATE_VALID

    def check(self, frame: bytes) -> tuple[str, bool]:
        """Process one received frame; return (status, data delivered to the application)."""
        if len(frame) != self.dlc or frame[0] != crc8_j1850(e2e_crc_input(self.data_id, frame)):
            self.counters["crc"] += 1
            self._error()
            return STATUS_CRC_ERROR, False
        counter = frame[1] & 0x0F
        if not self.cyclic or self.ref is None:
            self._ok(counter)
            return STATUS_OK, self.state == STATE_VALID
        delta = (counter - self.ref) % 16
        if delta == 0:
            self.counters["repeated"] += 1
            self._error()
            return STATUS_REPEATED, False
        if delta > self.max_delta:
            self.counters["sequence"] += 1
            self._error()
            self.ref = counter
            return STATUS_WRONG_SEQUENCE, False
        self._ok(counter)
        return STATUS_OK, self.state == STATE_VALID

    def timeout(self) -> None:
        """RX deadline missed: INVALID, reference cleared."""
        self.counters["timeout"] += 1
        self.state = STATE_INVALID
        self.ref = None
        self.ok_count = 0
        self.err_count = 0


# --------------------------------------------------------------------------- session crypto

LABEL_CLIENT = b"LSv1|cli"
LABEL_SERVER = b"LSv1|srv"
LABEL_SESSION = b"LSv1|session"
DIR_APP_TO_CGW = 0x41
DIR_CGW_TO_APP = 0x43
TAG_LEN = 16


def hmac_sha256(key: bytes, message: bytes) -> bytes:
    """HMAC-SHA256 (RFC 2104)."""
    return hmac.new(key, message, hashlib.sha256).digest()


def hkdf_sha256(ikm: bytes, salt: bytes, info: bytes, length: int) -> tuple[bytes, bytes]:
    """HKDF-SHA256 (RFC 5869); returns (PRK, OKM)."""
    prk = hmac_sha256(salt if salt else bytes(32), ikm)
    okm = b""
    block = b""
    index = 1
    while len(okm) < length:
        block = hmac_sha256(prk, block + info + bytes((index,)))
        okm += block
        index += 1
    return prk, okm[:length]


def client_proof(
    k_pair: bytes, device_id: bytes, server_nonce: bytes, client_nonce: bytes, client_id: bytes
) -> bytes:
    """HMAC-SHA256(K_pair, "LSv1|cli" || device_id || server_nonce || client_nonce || client_id)."""
    return hmac_sha256(k_pair, LABEL_CLIENT + device_id + server_nonce + client_nonce + client_id)


def server_proof(
    k_pair: bytes, device_id: bytes, server_nonce: bytes, client_nonce: bytes, client_id: bytes
) -> bytes:
    """HMAC-SHA256(K_pair, "LSv1|srv" || device_id || client_nonce || server_nonce || client_id)."""
    return hmac_sha256(k_pair, LABEL_SERVER + device_id + client_nonce + server_nonce + client_id)


def session_key(
    k_pair: bytes, device_id: bytes, server_nonce: bytes, client_nonce: bytes, client_id: bytes
) -> bytes:
    """K_sess = HKDF-SHA256(IKM K_pair, salt server_nonce || client_nonce, info label || ids, 32)."""
    return hkdf_sha256(
        k_pair, server_nonce + client_nonce, LABEL_SESSION + device_id + client_id, 32
    )[1]


def frame_tag(k_sess: bytes, direction: int, counter: int, body: bytes) -> bytes:
    """First 16 bytes of HMAC-SHA256(K_sess, dir || counter_BE32 || body)."""
    return hmac_sha256(k_sess, bytes((direction,)) + counter.to_bytes(4, "big") + body)[:TAG_LEN]


# --------------------------------------------------------------------------- protobuf wire format


def _varint(value: int) -> bytes:
    out = bytearray()
    value &= (1 << 64) - 1
    while True:
        byte = value & 0x7F
        value >>= 7
        if value:
            out.append(byte | 0x80)
        else:
            out.append(byte)
            return bytes(out)


def pb_varint(number: int, value: int) -> bytes:
    """Varint field (uint32, bool, enum; negative int32/enum values use 10 bytes)."""
    return _varint(number << 3) + _varint(value) if value else b""


def pb_sint32(number: int, value: int) -> bytes:
    """ZigZag-encoded sint32 field."""
    zigzag = (value << 1) ^ (value >> 31)
    return _varint(number << 3) + _varint(zigzag & 0xFFFFFFFF) if value else b""


def pb_bytes(number: int, value: bytes) -> bytes:
    """Length-delimited field (bytes, string or sub-message); proto3 omits empty values."""
    return _varint((number << 3) | 2) + _varint(len(value)) + value if value else b""


def pb_message(number: int, value: bytes) -> bytes:
    """Sub-message field; an empty sub-message that is set is still encoded."""
    return _varint((number << 3) | 2) + _varint(len(value)) + value
