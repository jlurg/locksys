# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Dart emitters for app/packages/locksys_protocol/lib/src/gen: parameters and DTC catalogue."""

from __future__ import annotations

from tools.codegen.common import banner, lower_camel
from tools.codegen.model import Catalogue, ParamGroup

SPDX = "// SPDX-License-Identifier: Apache-2.0\n// Copyright (c) 2026 jlurg\n"


def _dart_string(text: str) -> str:
    return "'" + text.replace("\\", "\\\\").replace("'", "\\'").replace("$", "\\$") + "'"


def params_library(groups: list[ParamGroup], source: str) -> str:
    """ls_params.dart: ``LsParams.<lowerCamelKey>`` constants."""
    lines = [banner(source, "dart").rstrip("\n"), SPDX.rstrip("\n"), ""]
    lines += [
        "/// Parameter registry of LS-SAIC-001 section 5.0 (`lim*` are verification limits).",
        "abstract final class LsParams {",
    ]
    for group in groups:
        lines += ["", f"  // {group.title} (LS-SAIC-001 {group.section})"]
        for p in group.params:
            kind = "bool" if isinstance(p.value, bool) else "int"
            value = str(p.value).lower() if isinstance(p.value, bool) else str(p.value)
            lines.append(f"  /// {p.desc} [{p.unit}].")
            lines.append(f"  static const {kind} {lower_camel(p.key)} = {value};")
    lines += ["}", ""]
    return "\n".join(lines)


def dtc_library(catalogue: Catalogue, source: str) -> str:
    """ls_dtc.dart: notice codes and CGW DTC values (``Notice.code`` of the APP protocol)."""
    lines = [banner(source, "dart").rstrip("\n"), SPDX.rstrip("\n"), ""]
    lines += [
        "/// Notice codes of LS-SAIC-001 section 8.9; codes >= 0x010000 are 24-bit DTC values.",
        "abstract final class LsNotices {",
    ]
    for notice in catalogue.notices:
        lines.append(f"  /// {notice.meaning} ({notice.severity}).")
        lines.append(
            f"  static const int {lower_camel(notice.name.lower())} = 0x{notice.code:04X};"
        )
    lines += [
        "}",
        "",
        "/// CGW DTC values (LS-SAIC-001 section 13.3), sent as `Notice.code`.",
        "abstract final class LsCgwDtcs {",
    ]
    for dtc in (d for d in catalogue.dtcs if d.node == "CGW"):
        lines.append(f"  /// {dtc.code}: {dtc.name} ({dtc.severity}).")
        lines.append(
            f"  static const int {dtc.symbol.replace('_', '').lower()} = 0x{dtc.value:06X};"
        )
    lines += [
        "",
        "  /// Display names, keyed by DTC value.",
        "  static const Map<int, String> names = {",
    ]
    for dtc in (d for d in catalogue.dtcs if d.node == "CGW"):
        lines.append(f"    0x{dtc.value:06X}: {_dart_string(dtc.code + ' ' + dtc.name)},")
    lines += ["  };", "}", ""]
    return "\n".join(lines)
