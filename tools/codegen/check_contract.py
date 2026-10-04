# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Check that the interface artefacts equal the normative content of LS-SAIC-001.

LS-SAIC-001 section 0.3: the files under interfaces/ are the source of code generation and
their content is identical to the normative tables and listings of the contract. Compared:
DBC (7.9), proto (8.6) and nanopb options (8.6), parameter registry (5.0), enumerations and
signal map (6.4), DTC catalogue (13.2, 13.3), notices (8.9) and the UART telemetry tables,
grammar and examples (9), whose checksums are also recomputed.

Usage: uv run tools/codegen/check_contract.py
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.codegen import saic
from tools.codegen.common import REPO_ROOT, CodegenError
from tools.codegen.model import (
    Catalogue,
    Dtc,
    EnumSet,
    Inhibit,
    Param,
    all_params,
    load_dtcs,
    load_enums,
    load_params,
)

DBC = Path("interfaces/can/locksys.dbc")
PROTO = Path("interfaces/proto/locksys/app/v1/locksys_app.proto")
OPTIONS = Path("interfaces/proto/locksys/app/v1/locksys_app.options")
PARAMS = Path("interfaces/params/timing.yaml")
ENUMS = Path("interfaces/enums/locksys_enums.yaml")
DTCS = Path("interfaces/dtc/dtc_catalog.yaml")
TELEMETRY = Path("interfaces/uart/telemetry_v1.md")

#: Options line that enforces static allocation; it has no row in the contract table.
STATIC_GUARD = ("locksys.app.v1.*", "type:FT_STATIC")


#: Licence header carried by the proto source but not by the contract listing.
PROTO_LICENCE_HEADER = "// SPDX-License-Identifier: Apache-2.0\n// Copyright (c) 2026 jlurg\n\n"


def strip_licence_header(text: str) -> str:
    """Return ``text`` without the leading SPDX licence block, when present."""
    return text.removeprefix(PROTO_LICENCE_HEADER)


def listing_problems(doc: str, repo_root: Path) -> list[str]:
    """DBC and proto files must equal the contract listings byte for byte.

    The proto file may start with the SPDX licence block, which the listing omits.
    """
    problems = []
    for path, lang in ((DBC, "dbc"), (PROTO, "proto")):
        text = (repo_root / path).read_text(encoding="utf-8")
        if lang == "proto":
            text = strip_licence_header(text)
        if text != saic.fenced_block(doc, lang):
            problems.append(f"{path}: differs from the ```{lang} listing of LS-SAIC-001")
    return problems


def options_problems(doc: str, repo_root: Path) -> list[str]:
    """nanopb options must equal the contract table (plus the static-allocation guard)."""
    text = (repo_root / OPTIONS).read_text(encoding="utf-8")
    lines = [re.sub(r"#.*", "", line).strip() for line in text.splitlines()]
    actual: dict[str, str] = {}
    guard = False
    for line in filter(None, lines):
        name, options = line.split(None, 1)
        if (name, options.strip()) == STATIC_GUARD:
            guard = True
            continue
        actual[name.removeprefix("locksys.app.v1.")] = " ".join(options.split())
    problems = [] if guard else [f"{OPTIONS}: static-allocation guard line missing"]
    expected = saic.parse_nanopb_options(doc)
    if actual != expected:
        problems.append(f"{OPTIONS}: {actual} differs from LS-SAIC-001 8.6 {expected}")
    return problems


def _param_text(param: Param) -> str:
    refs = [r for r in param.refs if r.startswith("SM-")]
    if param.key.startswith("lim_") or not refs:
        return param.desc
    return f"{param.desc} ({', '.join(refs)})"


def param_problems(doc: str, params: dict[str, Param]) -> list[str]:
    """Parameter values, units, ranges, owners, stages, cal flags and descriptions."""
    problems = []
    rows = saic.parse_params(doc)
    for key in sorted(set(rows) ^ set(params)):
        where = "YAML" if key in params else "LS-SAIC-001 5.0"
        problems.append(f"{PARAMS}: {key} only in the {where}")
    for key in sorted(set(rows) & set(params)):
        row, param = rows[key], params[key]
        mine = (param.value, param.unit, param.minimum, param.maximum, param.owner, param.stage)
        theirs = (row.value, row.unit, row.minimum, row.maximum, row.owner, row.stage)
        if mine != theirs or type(param.value) is not type(row.value):
            problems.append(f"{PARAMS}: {key} {mine} differs from LS-SAIC-001 {theirs}")
        if not key.startswith("lim_") and param.cal != row.cal:
            problems.append(f"{PARAMS}: {key} cal {param.cal} differs from LS-SAIC-001")
        if _param_text(param) != row.desc:
            problems.append(f"{PARAMS}: {key} description differs from LS-SAIC-001")
    return problems


def enum_problems(doc: str, enum_set: EnumSet) -> list[str]:
    """Enumeration names, widths, values, notes and the signal map."""
    problems = []
    rows = saic.parse_enums(doc)
    if list(rows) != list(enum_set.enums):
        problems.append(
            f"{ENUMS}: enumerations {list(enum_set.enums)} differ from LS-SAIC-001 {list(rows)}"
        )
    for name in rows.keys() & enum_set.enums.keys():
        row, enum = rows[name], enum_set.enums[name]
        mine = (enum.bits, enum.used_by, [(v.value, v.name, v.note) for v in enum.values])
        theirs = (row.bits, row.used_by, [(v.value, v.name, v.note) for v in row.values])
        if mine != theirs:
            problems.append(f"{ENUMS}: {name} differs from LS-SAIC-001 6.4")
    if saic.parse_signal_map(doc) != enum_set.signal_map:
        problems.append(f"{ENUMS}: signal_map differs from LS-SAIC-001 6.4")
    return problems


def inhibit_text(inhibit: Inhibit) -> str:
    """Render an inhibit in the notation of the contract table."""
    if inhibit.target == "safe":
        return "SAFE"
    if inhibit.later is not None:
        return f"A: {inhibit.target}; {inhibit.later}"
    text = inhibit.target
    if inhibit.release is not None:
        text += f", {inhibit.release}"
        if inhibit.time is not None:
            time = f"`{inhibit.time}`" if isinstance(inhibit.time, str) else f"{inhibit.time} ms"
            text += f"({time})"
    if inhibit.note is not None:
        text += f" ({inhibit.note})"
    return text


def severity_text(dtc: Dtc) -> str:
    """Render a severity in the notation of the contract table."""
    if dtc.critical_at_reset_limit:
        return f"{dtc.severity} (CRITICAL at the reset limit)"
    if dtc.severity_later:
        later = sorted(dtc.severity_later.items())
        stages = ", ".join(stage for stage, _ in later)
        return f"A: {dtc.severity}; {stages}: {later[0][1]}"
    return dtc.severity


def dtc_problems(doc: str, catalogue: Catalogue) -> list[str]:
    """DTC and notice tables."""
    problems = []
    rows = saic.parse_dtcs(doc)
    if [r.code for r in rows] != [d.code for d in catalogue.dtcs]:
        problems.append(f"{DTCS}: DTC list or order differs from LS-SAIC-001 13.2/13.3")
    for row, dtc in zip(rows, catalogue.dtcs, strict=False):
        stage = (
            None
            if dtc.stage is None
            else dtc.stage + (f" ({dtc.stage_note})" if dtc.stage_note else "")
        )
        mine = (
            dtc.code,
            dtc.value,
            dtc.name,
            dtc.node,
            stage,
            severity_text(dtc),
            None if dtc.inhibit is None else inhibit_text(dtc.inhibit),
            dtc.detection,
            dtc.reaction,
            dtc.healing,
        )
        theirs = (
            row.code,
            row.value,
            row.name,
            row.node,
            row.stage,
            row.severity,
            row.inhibit,
            row.cells["detection"],
            row.cells["reaction"],
            row.cells["healing"],
        )
        for label, a, b in zip(
            (
                "code",
                "value",
                "name",
                "node",
                "stage",
                "severity",
                "inhibit",
                "detection",
                "reaction",
                "healing",
            ),
            mine,
            theirs,
            strict=True,
        ):
            if a != b:
                problems.append(f"{DTCS}: {dtc.code} {label} '{a}' differs from LS-SAIC-001 '{b}'")
    notices = [(n.code, n.name, n.severity, n.meaning) for n in catalogue.notices]
    expected = [(n.code, n.name, n.severity, n.meaning) for n in saic.parse_notices(doc)]
    if notices != expected:
        problems.append(f"{DTCS}: notices differ from LS-SAIC-001 8.9")
    return problems


def nmea_checksum(line: str) -> str:
    """XOR of the bytes between the start character and '*', as two uppercase hex digits."""
    value = 0
    for char in line[1 : line.index("*")]:
        value ^= ord(char)
    return f"{value:02X}"


def telemetry_problems(doc: str, repo_root: Path) -> list[str]:
    """Every table row and listing of LS-SAIC-001 section 9 appears in the telemetry document."""
    problems = []
    contract = saic.section(doc, "## 9. UART telemetry contract", "## 10. ")
    document = (repo_root / TELEMETRY).read_text(encoding="utf-8")
    doc_rows = saic.table_rows(document)
    for row in saic.table_rows(contract):
        if row not in doc_rows:
            problems.append(f"{TELEMETRY}: table row {row[0]!r} differs from LS-SAIC-001 9")
    doc_blocks = re.findall(r"^```text\n(.*?)^```", document, re.S | re.M)
    for block in re.findall(r"^```text\n(.*?)^```", contract, re.S | re.M):
        if block not in doc_blocks:
            problems.append(
                f"{TELEMETRY}: listing '{block.splitlines()[0]}' differs from LS-SAIC-001 9"
            )
    for block in doc_blocks:
        for line in block.splitlines():
            match = re.fullmatch(r"[$#!].*\*([0-9A-F]{2})", line)
            if match and nmea_checksum(line) != match.group(1):
                problems.append(
                    f"{TELEMETRY}: checksum of '{line}' should be {nmea_checksum(line)}"
                )
    return problems


def check(repo_root: Path = REPO_ROOT) -> list[str]:
    """Run every contract comparison."""
    doc = saic.load(repo_root)
    params = all_params(load_params(repo_root / PARAMS))
    return (
        listing_problems(doc, repo_root)
        + options_problems(doc, repo_root)
        + param_problems(doc, params)
        + enum_problems(doc, load_enums(repo_root / ENUMS))
        + dtc_problems(doc, load_dtcs(repo_root / DTCS, params))
        + telemetry_problems(doc, repo_root)
    )


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
        print("contract: interface artefacts equal LS-SAIC-001")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
