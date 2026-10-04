# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Check model constants and enumerations against the generated interface headers.

* VSC001 every model constant that mirrors a parameter (LS-DCU-SAD-001 section 8.5) has the
  value of the parameter in ``libs/ls_common/gen/ls_params_gen.h``;
* VSC002 every literal of a contract enumeration in the model has the value of the matching
  ``LS_<ENUM>_<LITERAL>`` macro in ``libs/ls_common/gen/ls_enums_gen.h``;
* VSC003 the parameters and enumerations referenced by this check exist in the headers.

Without a model only VSC003 runs. Exit status: 0 = consistent, 1 = findings, 2 = input error.
"""

from __future__ import annotations

import argparse
import re
import sys
import xml.etree.ElementTree as ET
from collections.abc import Mapping, Sequence
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.vs import vs_common as vc

# Model constant -> parameter macros whose sum is the expected value.
CONSTANT_PARAMS: Mapping[str, tuple[str, ...]] = {
    "kBrakeMs": ("LS_T_WIN_BRAKE_MS",),
    "kRevDeadMs": ("LS_T_WIN_REV_DEAD_MS",),
    "kPulseGuardMs": ("LS_T_LOCK_PULSE_GUARD_MS",),
    "kSettleMs": ("LS_T_LOCK_SETTLE_MS", "LS_T_DEBOUNCE_MS"),
    "kRetryPauseMs": ("LS_T_LOCK_RETRY_PAUSE_MS",),
    "kInitMaxMs": ("LS_T_INIT_MAX_MS",),
    "kModeHealMs": ("LS_T_MODE_HEAL_MS",),
}
ENUMERATIONS = ("NodeMode", "WindowState", "WindowStopReason", "DoorLockState", "CommandResult")

DEFINE_RE = re.compile(r"^#define\s+(LS_[A-Z0-9_]+)\s+\(+(?:\([A-Za-z_]\w*\))?(-?\d+)u?\)+", re.M)


def parse_defines(text: str) -> dict[str, int]:
    """Return the integer value of every ``#define LS_*`` of a generated header."""
    return {name: int(value) for name, value in DEFINE_RE.findall(text)}


def macro_prefix(enumeration: str) -> str:
    """Return the macro prefix of a contract enumeration (``WindowState`` -> ``LS_WINDOW_STATE_``)."""
    return "LS_" + re.sub(r"(?<!^)(?=[A-Z])", "_", enumeration).upper() + "_"


def _int(text: str | None) -> int | None:
    if text is None:
        return None
    try:
        return int(text.strip().rstrip("uU"), 0)
    except ValueError:
        return None


def check_headers(params: Mapping[str, int], enums: Mapping[str, int]) -> list[str]:
    """VSC003: the referenced parameters and enumerations exist."""
    findings = []
    for constant, keys in CONSTANT_PARAMS.items():
        for key in keys:
            if key not in params:
                findings.append(f"VSC003: parameter {key} of {constant} not in ls_params_gen.h")
    for enumeration in ENUMERATIONS:
        prefix = macro_prefix(enumeration)
        if not any(name.startswith(prefix) for name in enums):
            findings.append(f"VSC003: enumeration {enumeration} not in ls_enums_gen.h")
    return findings


def check_model(
    paths: list[Path], root: Path, params: Mapping[str, int], enums: Mapping[str, int]
) -> list[str]:
    """VSC001 and VSC002 over the model files."""
    findings = []
    literal_tags = set(vc.MODEL_SCHEMA["literal"])
    for item in vc.iter_model_elements(paths):
        rel = item.source.relative_to(root).as_posix()
        if item.kind == "constant" and item.name in CONSTANT_PARAMS:
            expected = sum(params.get(k, 0) for k in CONSTANT_PARAMS[item.name])
            actual = _int(vc.attribute(item.element, vc.VALUE_ATTRIBUTES))
            if actual != expected:
                findings.append(f"{rel}: VSC001: {item.name} = {actual}, expected {expected}")
        if item.kind == "enumeration" and item.name in ENUMERATIONS:
            prefix = macro_prefix(item.name)
            for child in item.element.iter():
                name = vc.attribute(child, vc.NAME_ATTRIBUTES)
                if vc.local_tag(child) not in literal_tags or name is None:
                    continue
                macro = prefix + name.upper()
                actual = _int(vc.attribute(child, vc.VALUE_ATTRIBUTES))
                if macro not in enums:
                    findings.append(f"{rel}: VSC002: {item.name}.{name} has no {macro}")
                elif actual != enums[macro]:
                    findings.append(
                        f"{rel}: VSC002: {item.name}.{name} = {actual}, expected {enums[macro]}"
                    )
    return findings


def main(argv: Sequence[str] | None = None) -> int:
    """Command-line entry point."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=vc.REPO_ROOT, help="repository root")
    args = parser.parse_args(argv)
    try:
        params = parse_defines((args.root / vc.PARAMS_HEADER).read_text(encoding="utf-8"))
        enums = parse_defines((args.root / vc.ENUMS_HEADER).read_text(encoding="utf-8"))
    except OSError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    findings = check_headers(params, enums)
    if vc.project_files(args.root):
        try:
            findings += check_model(vc.model_files(args.root), args.root, params, enums)
        except ET.ParseError as exc:
            print(f"error: {exc}", file=sys.stderr)
            return 2
    else:
        print("check_vs_constants: no model yet; header references checked")
    for finding in findings:
        print(finding)
    print(f"check_vs_constants: {len(findings)} finding(s)")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
