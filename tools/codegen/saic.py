# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Reader for the normative listings and tables of LS-SAIC-001 (docs/02_system/LS-SAIC.md).

The interface artefacts under ``interfaces/`` must equal these tables and listings
(LS-SAIC-001 section 0.3); check_contract.py compares them.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path

from tools.codegen.common import CodegenError

SAIC_PATH = Path("docs/02_system/LS-SAIC.md")

_MINUS = "\u2212"
_DASH = "\u2014"
_RANGE = "\u2013"

#: Units as written in the contract -> ASCII units used in interfaces/params/timing.yaml.
UNIT_MAP = {"µs": "us", "ratio×1000": "ratio_x1000"}


@dataclass(frozen=True)
class ParamRow:
    """One row of the parameter registry (LS-SAIC section 5.0)."""

    key: str
    value: int | bool
    unit: str
    minimum: int | None
    maximum: int | None
    owner: tuple[str, ...]
    stage: str
    cal: bool
    desc: str


@dataclass(frozen=True)
class EnumValue:
    """One value of a canonical enumeration."""

    value: int
    name: str
    note: str | None


@dataclass(frozen=True)
class EnumRow:
    """One canonical enumeration (LS-SAIC section 6.4)."""

    name: str
    bits: int | None
    values: tuple[EnumValue, ...]
    used_by: str


@dataclass(frozen=True)
class DtcRow:
    """One DTC of the catalogue (LS-SAIC sections 13.2 and 13.3)."""

    code: str
    value: int
    name: str
    node: str
    stage: str | None
    severity: str
    inhibit: str | None
    cells: dict[str, str] = field(default_factory=dict)


@dataclass(frozen=True)
class NoticeRow:
    """One non-DTC notice (LS-SAIC section 8.9)."""

    code: int
    name: str
    severity: str
    meaning: str


def load(repo_root: Path) -> str:
    """Return the contract text."""
    path = repo_root / SAIC_PATH
    try:
        return path.read_text(encoding="utf-8")
    except OSError as exc:
        raise CodegenError(f"{SAIC_PATH}: {exc}") from exc


def fenced_block(doc: str, lang: str) -> str:
    """Return the body of the first fenced code block with the given info string."""
    match = re.search(r"^```" + re.escape(lang) + r"\n(.*?)^```", doc, re.S | re.M)
    if match is None:
        raise CodegenError(f"{SAIC_PATH}: no ```{lang} listing")
    return match.group(1)


def section(doc: str, start: str, end: str) -> str:
    """Return the text between two heading prefixes (start inclusive, end exclusive)."""
    begin = doc.find(start)
    if begin < 0:
        raise CodegenError(f"{SAIC_PATH}: heading '{start}' not found")
    stop = doc.find(end, begin + len(start))
    if stop < 0:
        raise CodegenError(f"{SAIC_PATH}: heading '{end}' not found")
    return doc[begin:stop]


def table_rows(text: str) -> list[list[str]]:
    """Return the body rows of every Markdown table in ``text`` (header rows removed)."""
    rows: list[list[str]] = []
    lines = text.splitlines()
    for index, line in enumerate(lines):
        if not line.startswith("|"):
            continue
        cells = [cell.strip().replace("\\|", "|") for cell in re.split(r"(?<!\\)\|", line)[1:-1]]
        is_separator = all(re.fullmatch(r":?-{3,}:?", cell) for cell in cells)
        next_is_separator = index + 1 < len(lines) and re.match(r"^\|\s*:?-{3,}", lines[index + 1])
        if is_separator or next_is_separator:
            continue
        rows.append(cells)
    return rows


def unquote(cell: str) -> str:
    """Strip one level of Markdown code quoting."""
    return cell[1:-1] if len(cell) > 1 and cell.startswith("`") and cell.endswith("`") else cell


def _int(text: str) -> int:
    return int(text.replace(_MINUS, "-").replace(",", ""), 0)


def parse_params(doc: str) -> dict[str, ParamRow]:
    """Parse the parameter registry tables of section 5.0."""
    text = section(doc, "### 5.0 Parameter registry", "### 5.1 ")
    params: dict[str, ParamRow] = {}
    for cells in table_rows(text):
        key = unquote(cells[0])
        if not re.fullmatch(r"[a-z][a-z0-9_]*", key):
            continue
        if key in params:
            raise CodegenError(f"{SAIC_PATH}: duplicate parameter {key}")
        if len(cells) == 8:
            _, value, unit, span, owner, stage, cal, desc = cells
        elif len(cells) == 6:
            _, value, unit, owner, stage, desc = cells
            span, cal = _DASH, _DASH
        else:
            raise CodegenError(f"{SAIC_PATH}: parameter {key}: unexpected column count")
        minimum = maximum = None
        if span != _DASH:
            bounds = re.fullmatch(rf"([{_MINUS}-]?\d+){_RANGE}([{_MINUS}-]?\d+)", span)
            if bounds is None:
                raise CodegenError(f"{SAIC_PATH}: parameter {key}: bad range '{span}'")
            minimum, maximum = _int(bounds.group(1)), _int(bounds.group(2))
        parsed: int | bool = value == "true" if value in ("true", "false") else _int(value)
        params[key] = ParamRow(
            key=key,
            value=parsed,
            unit=UNIT_MAP.get(unit, unit),
            minimum=minimum,
            maximum=maximum,
            owner=tuple(part.strip() for part in owner.split(",")),
            stage=stage,
            cal=cal == "yes",
            desc=desc,
        )
    return params


def parse_enums(doc: str) -> dict[str, EnumRow]:
    """Parse the canonical enumeration table of section 6.4."""
    text = section(doc, "### 6.4 Canonical enumerations", "Signal-to-enumeration map")
    enums: dict[str, EnumRow] = {}
    for cells in table_rows(text):
        if len(cells) != 4 or not re.fullmatch(r"[A-Z][A-Za-z]+", cells[0]):
            continue
        name, bits, values, used_by = cells
        found = re.findall(r"(\d+) ([A-Z0-9_]+)(?: \(([^)]*)\))?", values)
        enums[name] = EnumRow(
            name=name,
            bits=None if bits == "proto" else int(bits),
            values=tuple(EnumValue(int(v), n, note or None) for v, n, note in found),
            used_by=used_by,
        )
    return enums


def parse_signal_map(doc: str) -> dict[str, str]:
    """Parse the signal-to-enumeration map of section 6.4."""
    text = section(doc, "Signal-to-enumeration map", "### 6.5 ")
    mapping: dict[str, str] = {}
    for cells in table_rows(text):
        if len(cells) != 2:
            continue
        for signal in cells[0].split(","):
            mapping[signal.strip()] = cells[1]
    return mapping


def _dtc_value(code: str) -> int:
    letters = {"P": 0, "C": 1, "B": 2, "U": 3}
    base, ftb = code.split("-")
    two = (letters[base[0]] << 14) | (int(base[1], 16) << 12) | int(base[2:], 16)
    return (two << 8) | int(ftb, 16)


def parse_dtcs(doc: str) -> list[DtcRow]:
    """Parse the DCU and CGW DTC tables of sections 13.2 and 13.3."""
    rows: list[DtcRow] = []
    for node, start, end in (
        ("DCU", "### 13.2 DCU DTCs", "### 13.3 "),
        ("CGW", "### 13.3 CGW DTCs", "### 13.4 "),
    ):
        for cells in table_rows(section(doc, start, end)):
            if not re.fullmatch(r"[PCBU][0-9A-F]{4}-[0-9A-F]{2}", cells[0]):
                continue
            if node == "DCU":
                code, value, name, stage, detection, reaction, severity, inhibit, healing = cells
            else:
                code, value, name, detection, reaction, severity, healing = cells
                stage, inhibit = None, None
            if _int(value) != _dtc_value(code):
                raise CodegenError(f"{SAIC_PATH}: DTC {code}: value {value} does not match")
            rows.append(
                DtcRow(
                    code=code,
                    value=_int(value),
                    name=name,
                    node=node,
                    stage=stage,
                    severity=severity,
                    inhibit=inhibit,
                    cells={"detection": detection, "reaction": reaction, "healing": healing},
                )
            )
    return rows


def dtc_value(code: str) -> int:
    """24-bit DTC value of a code such as ``B1A11-71`` (LS-SAIC section 13.1)."""
    return _dtc_value(code)


def parse_notices(doc: str) -> list[NoticeRow]:
    """Parse the notice table of section 8.9."""
    rows: list[NoticeRow] = []
    for cells in table_rows(section(doc, "### 8.9 Notices", "### 8.10 ")):
        if len(cells) == 4 and re.fullmatch(r"0x[0-9A-F]{4}", cells[0]):
            rows.append(NoticeRow(_int(cells[0]), cells[1], cells[2], cells[3]))
    return rows


def parse_e2e_vectors(doc: str) -> list[tuple[str, str, int | None, bytes]]:
    """Parse the E2E vector table of section 7.10: (frame, content, DataID, bytes)."""
    rows = []
    for cells in table_rows(section(doc, "### 7.10 E2E test vectors", "## 8. ")):
        if len(cells) != 4:
            continue
        frame, content, data_id, data = cells
        rows.append(
            (
                frame,
                content,
                None if data_id == _DASH else _int(data_id),
                bytes.fromhex(unquote(data)),
            )
        )
    return rows


def parse_session_vectors(doc: str) -> dict[str, str]:
    """Parse the session vector table of section 8.6: item -> value text."""
    text = section(doc, "Session vectors (computed", "### 8.7 ")
    return {cells[0]: cells[1] for cells in table_rows(text) if len(cells) == 2}


def parse_nanopb_options(doc: str) -> dict[str, str]:
    """Parse the nanopb option table of section 8.6: ``Message.field`` -> options."""
    text = section(doc, "nanopb options (`locksys_app.options`)", "Encoded sizes")
    options: dict[str, str] = {}
    for cells in table_rows(text):
        if len(cells) != 2:
            continue
        for item in re.findall(r"`([A-Za-z]+\.[a-z_]+)`", cells[0]):
            options[item] = unquote(cells[1])
    return options
