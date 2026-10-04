# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Load and validate the interface sources: enumerations, parameters, DTC catalogue, manifest."""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from tools.codegen.common import CodegenError, load_yaml, rel

STAGES = ("A", "B", "C", "D", "E", "LATER")
OWNERS = ("APP", "CGW", "DCU", "HIL", "SYS")
SEVERITIES = ("INFO", "WARNING", "DEGRADED", "CRITICAL")
INHIBIT_TARGETS = ("none", "window", "window_dir", "lock", "window_lock", "safe")
INHIBIT_RELEASES = ("heal", "timeout", "test_after", "clear")

#: Unit -> accepted key suffixes (LS-SAIC-001 section 5.0). ``count`` uses the ``n_`` prefix
#: and ``bool`` keys carry no unit suffix.
UNIT_SUFFIXES: dict[str, tuple[str, ...]] = {
    "ms": ("_ms",),
    "us": ("_us",),
    "mA": ("_ma",),
    "mV": ("_mv",),
    "mV/A": ("_mv_per_a",),
    "dV": ("_dv",),
    "cdeg": ("_cdeg",),
    "cdeg/s": ("_cdeg_s",),
    "%": ("_pct",),
    "Hz": ("_hz",),
    "counts": ("_counts", "_cpr"),
    "counts/s": ("_cps",),
    "bytes": ("_bytes",),
    "ratio_x1000": ("_x1000",),
    "level": ("_level",),
    "baud": ("_baud",),
}
ALL_SUFFIXES = tuple(sorted({s for group in UNIT_SUFFIXES.values() for s in group}))


# --------------------------------------------------------------------------- parameters


@dataclass(frozen=True)
class Param:
    """One parameter of interfaces/params/timing.yaml."""

    key: str
    value: int | bool
    unit: str
    minimum: int | None
    maximum: int | None
    owner: tuple[str, ...]
    stage: str
    cal: bool
    desc: str
    refs: tuple[str, ...]
    group: str


@dataclass(frozen=True)
class ParamGroup:
    """A titled group of parameters (one subsection of LS-SAIC-001 section 5.0)."""

    name: str
    title: str
    section: str
    params: tuple[Param, ...]


def _require(condition: bool, where: str, message: str) -> None:
    if not condition:
        raise CodegenError(f"{where}: {message}")


def _check_param_name(key: str, unit: str, value: int | bool, where: str) -> None:
    _require(
        bool(re.fullmatch(r"[a-z][a-z0-9]*(?:_[a-z0-9]+)*", key)),
        where,
        "key is not lower_snake_case",
    )
    if unit == "bool":
        _require(isinstance(value, bool), where, "unit bool needs a true/false value")
        _require(not key.endswith(ALL_SUFFIXES), where, "boolean key must not carry a unit suffix")
        return
    _require(not isinstance(value, bool), where, f"unit {unit} needs an integer value")
    if unit == "count":
        _require(key.startswith("n_"), where, "unit count needs the n_ prefix")
        return
    _require(unit in UNIT_SUFFIXES, where, f"unknown unit '{unit}'")
    _require(
        key.endswith(UNIT_SUFFIXES[unit]), where, f"unit {unit} needs suffix {UNIT_SUFFIXES[unit]}"
    )


def _param(key: str, raw: Any, group: str, where: str) -> Param:
    _require(isinstance(raw, dict), where, "record must be a mapping")
    allowed = {"value", "unit", "min", "max", "owner", "stage", "cal", "desc", "refs"}
    unknown = set(raw) - allowed
    _require(not unknown, where, f"unknown fields {sorted(unknown)}")
    value = raw.get("value")
    _require(isinstance(value, int), where, "value must be an integer or a boolean")
    unit = raw.get("unit")
    _require(isinstance(unit, str), where, "unit missing")
    _check_param_name(key, str(unit), value, where)
    minimum, maximum = raw.get("min"), raw.get("max")
    _require((minimum is None) == (maximum is None), where, "min and max come together")
    if minimum is not None:
        _require(
            isinstance(minimum, int) and isinstance(maximum, int), where, "min/max must be integers"
        )
        _require(
            minimum <= value <= maximum, where, f"value {value} outside [{minimum}, {maximum}]"
        )
    owner = raw.get("owner")
    _require(isinstance(owner, list) and bool(owner), where, "owner must be a non-empty list")
    _require(all(o in OWNERS for o in owner), where, f"owner must be from {OWNERS}")
    stage = raw.get("stage")
    _require(stage in STAGES, where, f"stage must be one of {STAGES}")
    cal = raw.get("cal")
    _require(isinstance(cal, bool), where, "cal must be true or false")
    desc = raw.get("desc")
    _require(isinstance(desc, str) and bool(desc.strip()), where, "desc missing")
    refs = raw.get("refs", [])
    _require(isinstance(refs, list), where, "refs must be a list")
    if key.startswith("lim_"):
        _require(not cal, where, "verification limits are never calibration values")
    return Param(
        key=key,
        value=value,
        unit=str(unit),
        minimum=minimum,
        maximum=maximum,
        owner=tuple(owner),
        stage=str(stage),
        cal=cal,
        desc=str(desc),
        refs=tuple(str(r) for r in refs),
        group=group,
    )


def load_params(path: Path) -> list[ParamGroup]:
    """Load and validate interfaces/params/timing.yaml."""
    data = load_yaml(path)
    where = rel(path)
    _require(isinstance(data, dict) and data.get("version") == 1, where, "version: 1 expected")
    groups_raw = data.get("groups")
    _require(isinstance(groups_raw, dict) and bool(groups_raw), where, "groups missing")
    groups: list[ParamGroup] = []
    seen: set[str] = set()
    for name, body in groups_raw.items():
        gwhere = f"{where}: groups.{name}"
        _require(
            isinstance(body, dict) and isinstance(body.get("params"), dict),
            gwhere,
            "params missing",
        )
        params = []
        for key, raw in body["params"].items():
            _require(key not in seen, f"{gwhere}.{key}", "duplicate key")
            seen.add(key)
            params.append(_param(key, raw, name, f"{gwhere}.{key}"))
        groups.append(
            ParamGroup(
                name=name,
                title=str(body.get("title", name)),
                section=str(body.get("section", "")),
                params=tuple(params),
            )
        )
    return groups


def all_params(groups: list[ParamGroup]) -> dict[str, Param]:
    """Flatten parameter groups into a key -> Param mapping."""
    return {p.key: p for g in groups for p in g.params}


# --------------------------------------------------------------------------- enumerations


@dataclass(frozen=True)
class EnumValue:
    """One named value."""

    name: str
    value: int
    note: str | None


@dataclass(frozen=True)
class Enum:
    """One canonical enumeration of interfaces/enums/locksys_enums.yaml."""

    name: str
    bits: int | None
    proto: bool
    used_by: str
    values: tuple[EnumValue, ...]

    @property
    def snake(self) -> str:
        """Upper snake-case name, the proto value prefix (``WINDOW_STOP_REASON``)."""
        return re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "_", self.name).upper()


@dataclass(frozen=True)
class EnumSet:
    """All enumerations plus the signal map and the special raw values."""

    enums: dict[str, Enum]
    signal_map: dict[str, str]
    special_values: dict[str, tuple[int, str]] = field(default_factory=dict)


def load_enums(path: Path) -> EnumSet:
    """Load and validate interfaces/enums/locksys_enums.yaml."""
    data = load_yaml(path)
    where = rel(path)
    _require(isinstance(data, dict) and data.get("version") == 1, where, "version: 1 expected")
    enums: dict[str, Enum] = {}
    for name, body in (data.get("enums") or {}).items():
        ewhere = f"{where}: enums.{name}"
        _require(
            bool(re.fullmatch(r"[A-Z][A-Za-z0-9]*", name)), ewhere, "name must be UpperCamelCase"
        )
        _require(isinstance(body, dict), ewhere, "record must be a mapping")
        bits = body.get("bits")
        _require(
            bits is None or (isinstance(bits, int) and 1 <= bits <= 8), ewhere, "bits must be 1..8"
        )
        values = []
        for item in body.get("values") or []:
            _require(isinstance(item, dict), ewhere, "values must be mappings")
            vname, value = item.get("name"), item.get("value")
            _require(
                isinstance(vname, str) and bool(re.fullmatch(r"[A-Z][A-Z0-9_]*", vname)),
                ewhere,
                f"bad value name {vname!r}",
            )
            _require(isinstance(value, int) and value >= 0, ewhere, f"{vname}: value must be >= 0")
            values.append(EnumValue(str(vname), int(value), item.get("note")))
        _require(bool(values), ewhere, "no values")
        _require(any(v.value == 0 for v in values), ewhere, "value 0 (safe default) missing")
        _require(len({v.value for v in values}) == len(values), ewhere, "duplicate values")
        _require(len({v.name for v in values}) == len(values), ewhere, "duplicate names")
        _require(
            values == sorted(values, key=lambda v: v.value),
            ewhere,
            "values must be in ascending order",
        )
        if bits is not None:
            _require(
                max(v.value for v in values) < (1 << bits), ewhere, f"values exceed {bits} bits"
            )
        enums[name] = Enum(
            name=name,
            bits=bits,
            proto=bool(body.get("proto", False)),
            used_by=str(body.get("used_by", "")),
            values=tuple(values),
        )
    signal_map = {str(k): str(v) for k, v in (data.get("signal_map") or {}).items()}
    for signal, enum in signal_map.items():
        _require(enum in enums, f"{where}: signal_map.{signal}", f"unknown enumeration {enum}")
        _require(
            enums[enum].bits is not None,
            f"{where}: signal_map.{signal}",
            f"{enum} has no CAN width",
        )
    special = {}
    for signal, body in (data.get("special_values") or {}).items():
        _require(
            isinstance(body, dict) and isinstance(body.get("raw"), int),
            f"{where}: special_values.{signal}",
            "raw missing",
        )
        special[str(signal)] = (int(body["raw"]), str(body.get("name")))
    return EnumSet(enums=enums, signal_map=signal_map, special_values=special)


# --------------------------------------------------------------------------- DTC catalogue

_CODE = re.compile(r"([PCBU])([0-3])([0-9A-F])([0-9A-F]{2})-([0-9A-F]{2})")


def dtc_value(code: str) -> int:
    """24-bit value of a DTC code such as ``B1A11-71`` (LS-SAIC-001 section 13.1)."""
    match = _CODE.fullmatch(code)
    if match is None:
        raise CodegenError(f"invalid DTC code '{code}'")
    letter, d1, d2, rest, ftb = match.groups()
    two = ("PCBU".index(letter) << 14) | (int(d1) << 12) | (int(d2, 16) << 8) | int(rest, 16)
    return (two << 8) | int(ftb, 16)


@dataclass(frozen=True)
class Inhibit:
    """Per-DTC function inhibit (LS-SAIC-001 section 6.3)."""

    target: str
    release: str | None = None
    time: int | str | None = None
    note: str | None = None
    later: str | None = None


@dataclass(frozen=True)
class Dtc:
    """One DTC of interfaces/dtc/dtc_catalog.yaml."""

    code: str
    value: int
    node: str
    stage: str | None
    stage_note: str | None
    name: str
    severity: str
    critical_at_reset_limit: bool
    severity_later: dict[str, str]
    inhibit: Inhibit | None
    detection: str
    reaction: str
    healing: str

    @property
    def symbol(self) -> str:
        """Identifier part derived from the code (``B1A11_71``)."""
        return self.code.replace("-", "_")


@dataclass(frozen=True)
class Notice:
    """One non-DTC notice."""

    code: int
    name: str
    severity: str
    meaning: str


@dataclass(frozen=True)
class Catalogue:
    """DTCs and notices."""

    dtcs: tuple[Dtc, ...]
    notices: tuple[Notice, ...]
    availability_mask: int


def load_dtcs(path: Path, params: dict[str, Param]) -> Catalogue:
    """Load and validate interfaces/dtc/dtc_catalog.yaml against the parameter registry."""
    data = load_yaml(path)
    where = rel(path)
    _require(isinstance(data, dict) and data.get("version") == 1, where, "version: 1 expected")
    dtcs: list[Dtc] = []
    for index, raw in enumerate(data.get("dtcs") or []):
        dwhere = f"{where}: dtcs[{index}]"
        _require(isinstance(raw, dict), dwhere, "record must be a mapping")
        code = str(raw.get("code"))
        dwhere = f"{where}: {code}"
        _require(
            raw.get("value") == dtc_value(code), dwhere, f"value must be 0x{dtc_value(code):06X}"
        )
        node = raw.get("node")
        _require(node in ("DCU", "CGW"), dwhere, "node must be DCU or CGW")
        prefixes = ("B1A", "U1A") if node == "DCU" else ("B1B", "U1B")
        _require(code.startswith(prefixes), dwhere, f"{node} codes start with {prefixes}")
        severity = raw.get("severity")
        _require(severity in SEVERITIES, dwhere, f"severity must be one of {SEVERITIES}")
        later = raw.get("severity_later") or {}
        _require(
            all(s in STAGES and v in SEVERITIES for s, v in later.items()),
            dwhere,
            "bad severity_later",
        )
        stage = raw.get("stage")
        inhibit = None
        if node == "DCU":
            _require(stage in STAGES, dwhere, f"stage must be one of {STAGES}")
            inhibit_raw = raw.get("inhibit")
            _require(isinstance(inhibit_raw, dict), dwhere, "DCU DTCs need an inhibit")
            target, release, time = (
                inhibit_raw.get("target"),
                inhibit_raw.get("release"),
                inhibit_raw.get("time"),
            )
            _require(
                target in INHIBIT_TARGETS,
                dwhere,
                f"inhibit target must be one of {INHIBIT_TARGETS}",
            )
            _require(release is None or release in INHIBIT_RELEASES, dwhere, "bad inhibit release")
            _require(
                (target in ("none", "safe")) == (release is None),
                dwhere,
                "release needed exactly for function inhibits",
            )
            _require(
                (release in ("timeout", "test_after")) == (time is not None),
                dwhere,
                "time needed for timeout/test_after",
            )
            if isinstance(time, str):
                _require(
                    time in params and params[time].unit == "ms",
                    dwhere,
                    f"time {time} is not an ms parameter",
                )
            inhibit = Inhibit(
                str(target), release, time, inhibit_raw.get("note"), inhibit_raw.get("later")
            )
        else:
            _require(
                stage is None and "inhibit" not in raw, dwhere, "CGW DTCs have no stage or inhibit"
            )
        dtcs.append(
            Dtc(
                code=code,
                value=dtc_value(code),
                node=str(node),
                stage=stage,
                stage_note=raw.get("stage_note"),
                name=str(raw.get("name")),
                severity=str(severity),
                critical_at_reset_limit=bool(raw.get("critical_at_reset_limit", False)),
                severity_later={str(k): str(v) for k, v in later.items()},
                inhibit=inhibit,
                detection=str(raw.get("detection")),
                reaction=str(raw.get("reaction")),
                healing=str(raw.get("healing")),
            )
        )
    _require(len({d.code for d in dtcs}) == len(dtcs), where, "duplicate DTC codes")
    notices: list[Notice] = []
    for raw in data.get("notices") or []:
        code = raw.get("code")
        nwhere = f"{where}: notice {code}"
        _require(
            isinstance(code, int) and 0 < code < 0x010000, nwhere, "code must be 0x0001..0xFFFF"
        )
        _require(bool(re.fullmatch(r"[A-Z][A-Z0-9_]*", str(raw.get("name")))), nwhere, "bad name")
        _require(raw.get("severity") in SEVERITIES, nwhere, "bad severity")
        notices.append(
            Notice(int(code), str(raw["name"]), str(raw["severity"]), str(raw.get("meaning")))
        )
    _require(len({n.code for n in notices}) == len(notices), where, "duplicate notice codes")
    _require(len({n.name for n in notices}) == len(notices), where, "duplicate notice names")
    mask = (data.get("status") or {}).get("availability_mask")
    _require(isinstance(mask, int), where, "status.availability_mask missing")
    assert isinstance(mask, int)
    return Catalogue(tuple(dtcs), tuple(notices), mask)


# --------------------------------------------------------------------------- manifest


def load_manifest(path: Path) -> dict[str, Any]:
    """Load interfaces/can/codegen.yaml (the output manifest of every generator)."""
    data = load_yaml(path)
    where = rel(path)
    _require(isinstance(data, dict) and data.get("version") == 1, where, "version: 1 expected")
    for key in ("can", "proto", "enums", "params", "dtc", "vectors", "contract"):
        _require(isinstance(data.get(key), dict), where, f"section '{key}' missing")
    assert isinstance(data, dict)
    return data
