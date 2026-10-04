# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Cross-check the canonical enumerations: YAML <-> DBC VAL_ tables <-> proto enums.

Rules (LS-SAIC-001 section 6.4):
- every signal of the signal map has a VAL_ table equal to its enumeration and a bit width
  equal to the enumeration width;
- every other VAL_ table is a special value of interfaces/enums/locksys_enums.yaml;
- every enumeration marked ``proto: true`` exists in the proto with values
  ``<ENUM_NAME>_<VALUE> = <value>`` in the same order, and the proto has no other enumeration.

Usage: uv run tools/codegen/check_enums.py
"""

from __future__ import annotations

import re
import sys
from pathlib import Path
from typing import Any

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.codegen.common import REPO_ROOT, CodegenError
from tools.codegen.model import EnumSet, load_enums

ENUMS_YAML = Path("interfaces/enums/locksys_enums.yaml")
DBC = Path("interfaces/can/locksys.dbc")
PROTO = Path("interfaces/proto/locksys/app/v1/locksys_app.proto")


def proto_enums(text: str) -> dict[str, list[tuple[str, int]]]:
    """Top-level enumerations of a proto file: name -> [(value name, number)] in order."""
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    found: dict[str, list[tuple[str, int]]] = {}
    for name, body in re.findall(r"\benum\s+(\w+)\s*\{([^}]*)\}", text):
        found[name] = [(v, int(n)) for v, n in re.findall(r"(\w+)\s*=\s*(-?\d+)\s*;", body)]
    return found


def dbc_problems(enum_set: EnumSet, db: Any) -> list[str]:
    """Compare the DBC VAL_ tables with the enumerations and special values."""
    problems = []
    signals = {s.name: s for m in db.messages for s in m.signals}
    for signal_name, enum_name in enum_set.signal_map.items():
        signal = signals.get(signal_name)
        if signal is None:
            problems.append(f"signal_map: {signal_name} is not a DBC signal")
            continue
        enum = enum_set.enums[enum_name]
        expected = {v.value: v.name for v in enum.values}
        actual = {int(k): str(v) for k, v in (signal.choices or {}).items()}
        if actual != expected:
            problems.append(f"{signal_name}: VAL_ {actual} differs from {enum_name} {expected}")
        if signal.length != enum.bits:
            problems.append(f"{signal_name}: {signal.length} bits, {enum_name} has {enum.bits}")
    for signal_name, signal in signals.items():
        if not signal.choices or signal_name in enum_set.signal_map:
            continue
        special = enum_set.special_values.get(signal_name)
        actual = {int(k): str(v) for k, v in signal.choices.items()}
        if special is None:
            problems.append(f"{signal_name}: VAL_ table is neither mapped nor a special value")
        elif actual != {special[0]: special[1]}:
            problems.append(f"{signal_name}: VAL_ {actual} differs from special value {special}")
    for signal_name in enum_set.special_values:
        if signal_name not in signals:
            problems.append(f"special_values: {signal_name} is not a DBC signal")
    return problems


def proto_problems(enum_set: EnumSet, proto_text: str) -> list[str]:
    """Compare the proto enumerations with the YAML enumerations marked proto: true."""
    problems = []
    found = proto_enums(proto_text)
    for name, enum in enum_set.enums.items():
        if not enum.proto:
            if name in found:
                problems.append(f"{name}: in the proto but proto: false in the YAML")
            continue
        expected = [(f"{enum.snake}_{v.name}", v.value) for v in enum.values]
        if name not in found:
            problems.append(f"{name}: proto: true but missing in the proto")
        elif found[name] != expected:
            problems.append(f"{name}: proto values {found[name]} differ from {expected}")
    for name in found:
        if name not in enum_set.enums:
            problems.append(f"{name}: proto enumeration missing in the YAML")
    return problems


def check(repo_root: Path = REPO_ROOT) -> list[str]:
    """Run every cross-check and return the problems found."""
    import cantools

    enum_set = load_enums(repo_root / ENUMS_YAML)
    db = cantools.database.load_file(str(repo_root / DBC), strict=True)
    proto_text = (repo_root / PROTO).read_text(encoding="utf-8")
    return dbc_problems(enum_set, db) + proto_problems(enum_set, proto_text)


def main() -> int:
    """Command-line entry point."""
    try:
        problems = check()
    except CodegenError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    for problem in problems:
        print(f"error: {problem}", file=sys.stderr)
    if not problems:
        print("enums: YAML, DBC VAL_ tables and proto agree")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
