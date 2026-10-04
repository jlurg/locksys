# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Check the modelling rules of the Visual State model (LS-DCU-GDE-001 section 5).

* VSM001 names: events ``ev<Name>``, action functions ``a<Name>``, variables ``x<Name>`` or
  ``v<Name>``, constants ``k<Name>``;
* VSM002 events have no parameters;
* VSM003 the first action of every transition is ``aTr(...)``;
* VSM004 the three systems WinCtrl, DoorCtrl and ModeMgr exist.

Exit status: 0 = clean or no model, 1 = findings, 2 = unreadable model.
"""

from __future__ import annotations

import argparse
import re
import sys
import xml.etree.ElementTree as ET
from collections.abc import Sequence
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.vs import vs_common as vc

NAME_RULES = {
    "event": re.compile(r"^ev[A-Z]\w*$"),
    "action": re.compile(r"^a[A-Z]\w*$"),
    "variable": re.compile(r"^[xv][A-Z]\w*$"),
    "constant": re.compile(r"^k[A-Z]\w*$"),
}
SYSTEMS = ("WinCtrl", "DoorCtrl", "ModeMgr")
TRACE_ACTION = re.compile(r"^\s*aTr\s*\(")


def _first_action(element: ET.Element) -> str | None:
    text = vc.attribute(element, vc.ACTION_ATTRIBUTES)
    if text is None:
        for child in element:
            if vc.local_tag(child) in ("action", "actions"):
                text = (child.text or "").strip()
                break
    if text is None:
        return None
    return text.split(";", 1)[0].strip()


def check_model(root: Path) -> list[str]:
    """Return the findings of the model below ``root``."""
    files = vc.model_files(root)
    findings: list[str] = []
    systems = {p.stem for p in files if p.suffix == ".vsr"}
    for system in SYSTEMS:
        if system not in systems:
            findings.append(f"VSM004: system file {system}.vsr is missing")
    parameter_tags = set(vc.MODEL_SCHEMA["parameter"])
    for item in vc.iter_model_elements(files):
        rel = item.source.relative_to(root).as_posix()
        rule = NAME_RULES.get(item.kind)
        if item.kind == "event" and item.name in vc.BUILTIN_EVENTS:
            continue
        if rule is not None and not rule.match(item.name):
            findings.append(f"{rel}: VSM001: {item.kind} name '{item.name}'")
        if item.kind == "event" and any(
            vc.local_tag(child) in parameter_tags for child in item.element.iter()
        ):
            findings.append(f"{rel}: VSM002: event '{item.name}' has parameters")
        if item.kind == "transition":
            action = _first_action(item.element)
            if action is None or not TRACE_ACTION.match(action):
                findings.append(
                    f"{rel}: VSM003: transition '{item.name}' does not start with aTr()"
                )
    return findings


def main(argv: Sequence[str] | None = None) -> int:
    """Command-line entry point."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=vc.REPO_ROOT, help="repository root")
    args = parser.parse_args(argv)
    if not vc.project_files(args.root):
        print("check_vs_model: no model yet")
        return 0
    try:
        findings = check_model(args.root)
    except ET.ParseError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    for finding in findings:
        print(finding)
    print(f"check_vs_model: {len(findings)} finding(s)")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
