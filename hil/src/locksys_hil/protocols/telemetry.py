# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""UART telemetry version 1 parser (``interfaces/uart/telemetry_v1.md``, LS-SAIC-001 section 9).

``parse_line`` checks one line (grammar, checksum, length, field formats) and decodes the
typed fields. ``TelemetryMonitor`` adds the stream checks of section 9.5: per-type sequence
continuity (gaps accounted for by ``DROPPED`` lines) and monotonic ``t_ms``.
"""

import re
from collections.abc import Iterable, Sequence
from dataclasses import dataclass, field
from typing import Final

from locksys_hil.errors import ProtocolError

LINE_RE: Final = re.compile(r"^(\$LS[A-Z]{3}|#LOG|!LSFI),(.*)\*([0-9A-F]{2})$")
"""Line regex after stripping CRLF (section 9.5)."""

MAX_DATA_LINE: Final = 82
"""Maximum ``$LS`` line length including CRLF."""
MAX_LOG_LINE: Final = 120
"""Maximum ``#LOG`` line length including CRLF."""
SEQ_MODULO: Final = 65536
T_MS_MODULO: Final = 1 << 32

_SEQ_RE: Final = re.compile(r"^\d{5}$")
_T_MS_RE: Final = re.compile(r"^\d{10}$")
_TEMP_RE: Final = re.compile(r"^([+-])(\d{1,3})\.(\d{2})$")
_KL30_RE: Final = re.compile(r"^\d{1,2}\.\d$")
_SPEED_RE: Final = re.compile(r"^([+-])(\d{1,4})\.(\d)$")
_COUNTS_RE: Final = re.compile(r"^[+-]\d{1,10}$")
_ENUM_RE: Final = re.compile(r"^[A-Z][A-Z0-9_]*$")
_HEX_RE: Final = re.compile(r"^[0-9A-F]+$")
_FORBIDDEN: Final = frozenset(",*$#!\r\n")

# Field count of each sentence type (section 9.3).
_FIELD_COUNT: Final[dict[str, int]] = {
    "$LSTMP": 4,
    "$LSSTA": 9,
    "$LSMOT": 7,
    "$LSVER": 5,
    "$LSRST": 2,
    "$LSDTC": 5,
    "#LOG": 6,
}
_SEQUENCED: Final = frozenset({"$LSTMP", "$LSSTA", "$LSMOT", "$LSDTC", "#LOG"})


def checksum(body: str) -> int:
    """Return the XOR of all characters between the start character and ``*`` (both excluded)."""
    value = 0
    for char in body.encode("ascii"):
        value ^= char
    return value


def build_line(kind: str, fields: Sequence[str]) -> str:
    """Build a line without CRLF, e.g. ``build_line("!LSFI", ["HANG", "500"])``.

    Raises:
        ValueError: a field contains a reserved character.
    """
    for item in fields:
        if _FORBIDDEN.intersection(item):
            raise ValueError(f"field {item!r} contains a reserved character")
    text = ",".join((kind, *fields))
    return f"{text}*{checksum(text[1:]):02X}"


def fault_injection_command(cmd: str, *args: int | str) -> str:
    """Build a ``!LSFI`` fault-injection line including CRLF (section 9.6)."""
    return build_line("!LSFI", [cmd, *(str(a) for a in args)]) + "\r\n"


@dataclass(frozen=True)
class Sentence:
    """One decoded telemetry line.

    Attributes:
        kind: ``$LSTMP`` ... ``$LSDTC``, ``#LOG`` or ``!LSFI``.
        fields: Raw fields.
        values: Typed values keyed by field name.
    """

    kind: str
    fields: tuple[str, ...]
    values: dict[str, object]

    @property
    def seq(self) -> int | None:
        """Sequence number, or None for sentences without one."""
        value = self.values.get("seq")
        return value if isinstance(value, int) else None

    @property
    def t_ms(self) -> int | None:
        """Uptime in ms (modulo 2^32), or None for sentences without one."""
        value = self.values.get("t_ms")
        return value if isinstance(value, int) else None


def _seq(text: str) -> int:
    if not _SEQ_RE.match(text) or int(text) >= SEQ_MODULO:
        raise ProtocolError(f"bad seq {text!r}")
    return int(text)


def _t_ms(text: str) -> int:
    if not _T_MS_RE.match(text) or int(text) >= T_MS_MODULO:
        raise ProtocolError(f"bad t_ms {text!r}")
    return int(text)


def parse_temp_cdeg(text: str) -> int | None:
    """Decode a ``temp`` field into cdeg (None when empty)."""
    if text == "":
        return None
    match = _TEMP_RE.match(text)
    if match is None:
        raise ProtocolError(f"bad temp {text!r}")
    value = int(match.group(2)) * 100 + int(match.group(3))
    return -value if match.group(1) == "-" else value


def parse_speed_x10(text: str) -> int | None:
    """Decode a ``speed`` field into 0.1 rpm (None when empty)."""
    if text == "":
        return None
    match = _SPEED_RE.match(text)
    if match is None:
        raise ProtocolError(f"bad speed {text!r}")
    value = int(match.group(2)) * 10 + int(match.group(3))
    return -value if match.group(1) == "-" else value


def _kl30_dv(text: str) -> int | None:
    if text == "":
        return None
    if not _KL30_RE.match(text):
        raise ProtocolError(f"bad kl30 {text!r}")
    return int(text.replace(".", ""))


def _pos(text: str) -> int | None:
    if text == "UNK":
        return None
    if not text.isdigit() or int(text) > 100:
        raise ProtocolError(f"bad pos {text!r}")
    return int(text)


def _enum(text: str) -> str:
    if not _ENUM_RE.match(text):
        raise ProtocolError(f"bad enum {text!r}")
    return text


def _hex(text: str, width: int) -> int:
    if len(text) != width or not _HEX_RE.match(text):
        raise ProtocolError(f"bad hex{width} {text!r}")
    return int(text, 16)


def _int(text: str, low: int, high: int) -> int:
    if not text.isdigit() or not low <= int(text) <= high:
        raise ProtocolError(f"bad integer {text!r}")
    return int(text)


def _counts(text: str) -> int:
    if not _COUNTS_RE.match(text):
        raise ProtocolError(f"bad counts {text!r}")
    value = int(text)
    if not -(1 << 31) <= value < (1 << 31):
        raise ProtocolError(f"counts out of range {text!r}")
    return value


def _git7(text: str) -> str:
    if not re.match(r"^[0-9a-f]{7}$", text):
        raise ProtocolError(f"bad git7 {text!r}")
    return text


def _decode(kind: str, f: Sequence[str]) -> dict[str, object]:
    if kind == "$LSTMP":
        return {
            "seq": _seq(f[0]),
            "t_ms": _t_ms(f[1]),
            "temp_cdeg": parse_temp_cdeg(f[2]),
            "temp_status": _enum(f[3]),
        }
    if kind == "$LSSTA":
        return {
            "seq": _seq(f[0]),
            "t_ms": _t_ms(f[1]),
            "node_mode": _enum(f[2]),
            "door_lock_state": _enum(f[3]),
            "window_state": _enum(f[4]),
            "pos_pct": _pos(f[5]),
            "kl30_dv": _kl30_dv(f[6]),
            "dtc_count": _int(f[7], 0, 255),
            "speed_rpm_x10": parse_speed_x10(f[8]),
        }
    if kind == "$LSMOT":
        return {
            "seq": _seq(f[0]),
            "t_ms": _t_ms(f[1]),
            "window_state": _enum(f[2]),
            "duty_pct": _int(f[3], 0, 100),
            "speed_rpm_x10": parse_speed_x10(f[4]),
            "pos_counts": _counts(f[5]),
            "encoder_status": _enum(f[6]),
        }
    if kind == "$LSVER":
        return {
            "sw_version": f[0],
            "git7": _git7(f[1]),
            "can_matrix": f[2],
            "tlm_version": _int(f[3], 0, 255),
            "build_type": _enum(f[4]),
        }
    if kind == "$LSRST":
        return {"reset_reason": _enum(f[0]), "reset_count": _int(f[1], 0, 1 << 32)}
    if kind == "$LSDTC":
        if f[4] not in {"SET", "CLR"}:
            raise ProtocolError(f"bad DTC transition {f[4]!r}")
        return {
            "seq": _seq(f[0]),
            "t_ms": _t_ms(f[1]),
            "dtc": _hex(f[2], 6),
            "status": _hex(f[3], 2),
            "transition": f[4],
        }
    if kind == "#LOG":
        if f[2] not in {"E", "W", "I", "D"}:
            raise ProtocolError(f"bad log level {f[2]!r}")
        if not re.match(r"^[A-Z]{2,4}$", f[3]):
            raise ProtocolError(f"bad log module {f[3]!r}")
        if len(f[5]) > 40 or not f[5].isprintable():
            raise ProtocolError("bad log text")
        return {
            "seq": _seq(f[0]),
            "t_ms": _t_ms(f[1]),
            "level": f[2],
            "module": f[3],
            "code": _hex(f[4], 4),
            "text": f[5],
        }
    raise ProtocolError(f"unknown sentence {kind}")


def parse_line(line: str) -> Sentence:
    """Parse and check one telemetry line (with or without CRLF).

    Raises:
        ProtocolError: grammar, checksum, length or field format violation.
    """
    length = len(line) if line.endswith("\r\n") else len(line.rstrip("\r\n")) + 2
    text = line.rstrip("\r\n")
    match = LINE_RE.match(text)
    if match is None:
        raise ProtocolError(f"line does not match the grammar: {text!r}")
    kind, body, cs_text = match.groups()
    if checksum(text[1 : text.rindex("*")]) != int(cs_text, 16):
        raise ProtocolError(f"checksum mismatch: {text!r}")
    limit = MAX_LOG_LINE if kind == "#LOG" else MAX_DATA_LINE
    if length > limit:
        raise ProtocolError(f"line longer than {limit} characters: {text!r}")
    fields = tuple(body.split(","))
    if kind == "!LSFI":
        return Sentence(kind, fields, {"cmd": fields[0], "args": fields[1:]})
    expected = _FIELD_COUNT.get(kind)
    if expected is None:
        raise ProtocolError(f"unknown sentence {kind}")
    if len(fields) != expected:
        raise ProtocolError(f"{kind} has {len(fields)} fields, expected {expected}")
    return Sentence(kind, fields, _decode(kind, fields))


@dataclass
class TelemetryMonitor:
    """Stream checks over consecutive lines (LS-SAIC-001 section 9.5).

    Lines are fed in reception order. Malformed lines, backwards ``t_ms`` steps and sequence
    gaps are recorded in ``issues``; gaps are acceptable up to the number of lines reported
    by ``#LOG,...,TLM,0001,DROPPED n`` lines (checked by ``finish``).
    """

    sentences: list[Sentence] = field(default_factory=list)
    issues: list[str] = field(default_factory=list)
    gap_lines: int = 0
    dropped_lines: int = 0
    _last_seq: dict[str, int] = field(default_factory=dict)
    _last_t_ms: int | None = None

    def feed(self, line: str) -> Sentence | None:
        """Check one line; returns the sentence, or None when the line is malformed."""
        try:
            sentence = parse_line(line)
        except ProtocolError as exc:
            self.issues.append(str(exc))
            return None
        self.sentences.append(sentence)
        self._check_sequence(sentence)
        self._check_time(sentence)
        if (
            sentence.kind == "#LOG"
            and sentence.values.get("module") == "TLM"
            and sentence.values.get("code") == 1
        ):
            text = str(sentence.values.get("text", ""))
            match = re.match(r"^DROPPED (\d+)$", text)
            if match:
                self.dropped_lines += int(match.group(1))
        return sentence

    def feed_all(self, lines: Iterable[str]) -> list[str]:
        """Feed several lines and return ``finish()``."""
        for line in lines:
            self.feed(line)
        return self.finish()

    def finish(self) -> list[str]:
        """Return all issues, including sequence gaps not covered by DROPPED lines."""
        issues = list(self.issues)
        if self.gap_lines > self.dropped_lines:
            issues.append(
                f"{self.gap_lines} missing lines, only {self.dropped_lines} reported as DROPPED"
            )
        return issues

    def _check_sequence(self, sentence: Sentence) -> None:
        seq = sentence.seq
        if sentence.kind not in _SEQUENCED or seq is None:
            return
        last = self._last_seq.get(sentence.kind)
        self._last_seq[sentence.kind] = seq
        if last is None:
            return
        step = (seq - last) % SEQ_MODULO
        if step == 0:
            self.issues.append(f"{sentence.kind} seq {seq} repeated")
        elif step > 1:
            self.gap_lines += step - 1

    def _check_time(self, sentence: Sentence) -> None:
        t_ms = sentence.t_ms
        if t_ms is None:
            return
        if self._last_t_ms is not None:
            step = (t_ms - self._last_t_ms) % T_MS_MODULO
            if step >= T_MS_MODULO // 2:
                self.issues.append(f"t_ms went backwards: {self._last_t_ms} -> {t_ms}")
        self._last_t_ms = t_ms
