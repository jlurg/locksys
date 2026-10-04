# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Python emitters for hil/src/locksys_hil/gen: enumerations, parameters, DTC catalogue."""

from __future__ import annotations

from tools.codegen.common import banner
from tools.codegen.model import Catalogue, EnumSet, ParamGroup

SPDX = "# SPDX-License-Identifier: Apache-2.0\n# Copyright (c) 2026 jlurg\n"


def _head(source: str, doc: str) -> list[str]:
    return [banner(source, "hash").rstrip("\n"), SPDX.rstrip("\n"), f'"""{doc}"""', ""]


def package_init(source: str) -> str:
    """hil/src/locksys_hil/gen/__init__.py."""
    lines = [
        banner(source, "hash").rstrip("\n"),
        SPDX.rstrip("\n"),
        '"""Interface constants and protobuf stubs generated from interfaces/ (never edit)."""',
        "",
    ]
    return "\n".join(lines)


def enums_module(enum_set: EnumSet, source: str) -> str:
    """hil/src/locksys_hil/gen/enums.py."""
    lines = _head(source, "Canonical enumerations of LS-SAIC-001 section 6.4.")
    lines += ["from enum import IntEnum", "from typing import Final", ""]
    for enum in enum_set.enums.values():
        width = "proto only" if enum.bits is None else f"CAN {enum.bits} bits"
        lines += [
            "",
            f"class {enum.name}(IntEnum):",
            f'    """{enum.name} ({width}; used by {enum.used_by})."""',
            "",
        ]
        for value in enum.values:
            note = f"  # {value.note}" if value.note else ""
            lines.append(f"    {value.name} = {value.value}{note}")
        lines.append("")
    lines += [
        "",
        "#: CAN signal -> enumeration (input of the enum cross-check).",
        "SIGNAL_ENUMS: Final[dict[str, type[IntEnum]]] = {",
    ]
    lines += [f'    "{signal}": {enum},' for signal, enum in enum_set.signal_map.items()]
    lines += [
        "}",
        "",
        "#: Special raw values of numeric signals: signal -> (raw, name).",
        "SPECIAL_VALUES: Final[dict[str, tuple[int, str]]] = {",
    ]
    lines += [
        f'    "{signal}": ({raw}, "{name}"),'
        for signal, (raw, name) in enum_set.special_values.items()
    ]
    lines += ["}", ""]
    return "\n".join(lines)


def params_module(groups: list[ParamGroup], source: str) -> str:
    """hil/src/locksys_hil/gen/params.py."""
    lines = _head(
        source, "Parameter registry of LS-SAIC-001 section 5.0 (LIM_* are verification limits)."
    )
    lines += ["from typing import Final", ""]
    ranges: list[str] = []
    units: list[str] = []
    for group in groups:
        lines += ["", f"# {group.title} (LS-SAIC-001 {group.section})"]
        for p in group.params:
            value = str(p.value)
            lines.append(f"{p.key.upper()}: Final = {value}")
            refs = f" ({', '.join(p.refs)})" if p.refs else ""
            lines.append(f'"""{p.desc} [{p.unit}]{refs}."""'.replace("\\", "\\\\"))
            units.append(f'    "{p.key}": "{p.unit}",')
            if p.minimum is not None:
                ranges.append(f'    "{p.key}": ({p.minimum}, {p.maximum}),')
    lines += [
        "",
        "",
        "#: Calibration range or tolerance of the keys that define one.",
        "RANGES: Final[dict[str, tuple[int, int]]] = {",
        *ranges,
        "}",
    ]
    lines += ["", "#: Unit of every key.", "UNITS: Final[dict[str, str]] = {", *units, "}", ""]
    return "\n".join(lines)


def dtc_module(catalogue: Catalogue, source: str) -> str:
    """hil/src/locksys_hil/gen/dtc.py."""
    lines = _head(source, "DTC and notice catalogue of LS-SAIC-001 sections 8.9 and 13.")
    lines += [
        "from dataclasses import dataclass",
        "from typing import Final",
        "",
        "",
        "@dataclass(frozen=True)",
        "class Dtc:",
        '    """One DTC: code, 24-bit value, node, name and severity (FaultSeverity name)."""',
        "",
        "    code: str",
        "    value: int",
        "    node: str",
        "    name: str",
        "    severity: str",
        "",
        "",
        "@dataclass(frozen=True)",
        "class Notice:",
        '    """One non-DTC notice."""',
        "",
        "    code: int",
        "    name: str",
        "    severity: str",
        "    meaning: str",
        "",
        "",
        f"STATUS_AVAILABILITY_MASK: Final = 0x{catalogue.availability_mask:02X}",
        "",
    ]
    for dtc in catalogue.dtcs:
        lines.append(f"{dtc.symbol}: Final = 0x{dtc.value:06X}")
    lines += ["", "DTCS: Final[tuple[Dtc, ...]] = ("]
    for dtc in catalogue.dtcs:
        lines.append(
            f'    Dtc("{dtc.code}", 0x{dtc.value:06X}, "{dtc.node}", {dtc.name!r}, "{dtc.severity}"),'
        )
    lines += [")", ""]
    for notice in catalogue.notices:
        lines.append(f"NOTICE_{notice.name}: Final = 0x{notice.code:04X}")
    lines += ["", "NOTICES: Final[tuple[Notice, ...]] = ("]
    for notice in catalogue.notices:
        lines.append(
            f'    Notice(0x{notice.code:04X}, "{notice.name}", "{notice.severity}", {notice.meaning!r}),'
        )
    lines += [")", ""]
    return "\n".join(lines)
