# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
# /// script
# requires-python = ">=3.13"
# dependencies = []
# ///
"""Requirement traceability matrix and gate (LS-VER-001 section 10).

Sources:

* Identifiers: first column of the Markdown tables under ``docs/``. A table whose first header
  cell is ``ID`` defines items and provides their attributes (title, parents, verification,
  stage, tags, milestone); other tables only reference identifiers.
* Code tags in source and test directories only (documentation examples are never read):
  C/C++/Dart ``@satisfies ID`` and ``@verifies ID``; Python ``pytest.mark.verifies("ID")`` and
  ``pytest.mark.test_id("ID")``.
* Optional JUnit XML files whose test cases carry ``verifies`` properties.

Rules T1-T7 of LS-VER-001 section 10.2; ``--phase`` selects the blocking set of section 10.3.

Outputs ``trace_matrix.csv``, ``trace_matrix.json`` and ``trace_matrix.md`` in ``--out-dir``.

Exit status: ``--report`` always 0; ``--gate`` 0 = no blocking violation, 1 = violations,
2 = input error.
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import re
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict
from collections.abc import Iterable, Iterator, Sequence
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

ID_RE = re.compile(
    r"(?:(?:STK|SYS|HWR|CSR)-\d{3}"
    r"|SWR-(?:DCU|CGW|APP|LIB)-\d{3}"
    r"|(?:HAZ|SG|SM|AS|TS|CSG)-\d{2}"
    r"|TST-(?:UT|IT|HIL|MAN)-(?:DCU|CGW|APP|SYS|LIB)-\d{3})"
)
"""Identifier formats of LS-VER-001 section 10.1."""

ID_LIKE_RE = re.compile(r"\b(?:STK|SYS|HWR|CSR|SWR|HAZ|SG|SM|AS|TS|CSG|TST)(?:-[A-Z0-9]+)+\b")
"""Anything that looks like an identifier (used to report malformed tags)."""

C_TAG_RE = re.compile(r"@(satisfies|verifies)\b([^*\n]*)")
PY_MARK_RE = re.compile(r"\bmark\.(verifies|test_id)\(([^)]*)\)")
FENCE_RE = re.compile(r"^\s*(```|~~~)")
SEPARATOR_RE = re.compile(r"^\|?\s*:?-{3,}:?\s*(\|\s*:?-{3,}:?\s*)*\|?\s*$")

SCAN_ROOTS = ("firmware", "libs", "app", "hil", "tools")
"""Source and test directories searched for tags (docs/ is never searched)."""
SKIP_PARTS = frozenset(
    {
        "gen",
        "gen_vs",
        "build",
        "third_party",
        ".dart_tool",
        ".venv",
        "node_modules",
        "managed_components",
        ".fvm",
        "Pods",
        "__pycache__",
    }
)
SKIP_DIRS = (Path("tools") / "trace",)
"""The trace tool and its fixtures contain example tags."""
SOURCE_SUFFIXES = frozenset({".c", ".h", ".cpp", ".hpp", ".py", ".dart"})
INDEX_DOCS = ("LS-SAIC.md", "amendment_register.md")
"""Documents that only summarise identifiers defined elsewhere; read last."""

PHASE_RULES: dict[int, tuple[str, ...]] = {
    1: (),
    2: ("T1", "T2", "T3:DCU"),
    3: ("T1", "T2", "T3:DCU", "T3:CGW", "T3:APP"),
    4: ("T1", "T2", "T3:DCU", "T3:CGW", "T3:APP", "T3:LIB", "T4", "T5", "T6"),
}
"""Blocking rules per phase (LS-VER-001 section 10.3); T7 is added by ``--release``."""


def kind_of(identifier: str) -> str:
    """Return the identifier class: ``SYS``, ``SWR-DCU``, ``SM``, ``TST-HIL`` ..."""
    parts = identifier.split("-")
    return "-".join(parts[:2]) if parts[0] in {"SWR", "TST"} else parts[0]


def _clean_cell(cell: str) -> str:
    return cell.strip().strip("`*_ ").strip()


def _split_row(line: str) -> list[str]:
    text = line.strip()
    text = text.removeprefix("|")
    text = text.removesuffix("|")
    return [c.strip() for c in re.split(r"(?<!\\)\|", text)]


def _ids_in(text: str) -> list[str]:
    return [m.group(0) for m in ID_RE.finditer(text)]


@dataclass
class Item:
    """One defined identifier with its attributes."""

    id: str
    kind: str
    title: str = ""
    doc: str = ""
    parents: set[str] = field(default_factory=set)
    implemented_by: set[str] = field(default_factory=set)
    verifies: set[str] = field(default_factory=set)
    ver: str = ""
    stage: str = ""
    tags: str = ""
    milestone: str = ""
    defined_in: list[str] = field(default_factory=list)

    @property
    def is_mvp(self) -> bool:
        """MVP = stage A (or no stage) and not scheduled LATER."""
        if "LATER" in self.milestone.upper() or "LATER" in self.stage.upper():
            return False
        stages = {s.strip() for s in re.split(r"[,/ ]+", self.stage) if s.strip()}
        stages.discard("—")
        stages.discard("-")
        return not stages or "A" in stages

    @property
    def is_safety(self) -> bool:
        """True for [SAF] items."""
        return "SAF" in self.tags

    def methods(self) -> set[str]:
        """Verification methods of the Ver column (HIL, IT, UT, R, M)."""
        return {m for m in re.split(r"[,; ]+", self.ver.upper()) if m}


@dataclass(frozen=True)
class Tag:
    """One code tag."""

    relation: str
    id: str
    path: str
    line: int
    level: str
    """``impl`` for @satisfies; ``UT``, ``IT`` or ``HIL`` for verification tags."""


@dataclass(frozen=True)
class JunitResult:
    """Outcome of one JUnit test case that carries identifiers."""

    name: str
    ids: tuple[str, ...]
    passed: bool
    hil: bool


@dataclass(frozen=True)
class Violation:
    """One rule violation."""

    rule: str
    id: str
    message: str


_ATTRIBUTE_COLUMNS = {
    "requirement": "title",
    "title": "title",
    "mechanism": "title",
    "goal": "title",
    "safety goal": "title",
    "hazard": "title",
    "threat": "title",
    "asset": "title",
    "statement": "title",
    "ver": "ver",
    "verification": "ver",
    "stage": "stage",
    "tag": "tags",
    "ms": "milestone",
    "milestone": "milestone",
}
_PARENT_COLUMNS = frozenset({"parents", "parent", "goals"})


def parse_docs(docs_dir: Path, root: Path) -> dict[str, Item]:
    """Collect identifier definitions from the Markdown tables under ``docs_dir``."""
    paths = sorted(docs_dir.rglob("*.md"), key=lambda p: (p.name in INDEX_DOCS, str(p)))
    items: dict[str, Item] = {}
    for path in paths:
        rel = path.relative_to(root).as_posix()
        for header, row, number in _table_rows(path.read_text(encoding="utf-8")):
            first = _clean_cell(row[0]) if row else ""
            if not ID_RE.fullmatch(first) or header[0].lower() != "id":
                continue
            item = items.setdefault(first, Item(id=first, kind=kind_of(first)))
            item.defined_in.append(f"{rel}:{number}")
            item.doc = item.doc or rel
            _apply_columns(item, header, row)
    return items


def _apply_columns(item: Item, header: list[str], row: list[str]) -> None:
    for name, cell in zip(header[1:], row[1:], strict=False):
        key = name.strip().lower()
        value = cell.strip()
        if key in _PARENT_COLUMNS:
            item.parents.update(_ids_in(value))
        elif key == "requirements" and item.kind == "SM":
            item.implemented_by.update(_ids_in(value))
        elif key == "verifies":
            item.verifies.update(_ids_in(value))
        elif key in _ATTRIBUTE_COLUMNS:
            attribute = _ATTRIBUTE_COLUMNS[key]
            if not getattr(item, attribute):
                setattr(item, attribute, value)


def _table_rows(text: str) -> Iterator[tuple[list[str], list[str], int]]:
    lines = text.splitlines()
    in_fence = False
    header: list[str] | None = None
    for index, line in enumerate(lines):
        if FENCE_RE.match(line):
            in_fence = not in_fence
            header = None
            continue
        if in_fence or not line.lstrip().startswith("|"):
            header = None
            continue
        if SEPARATOR_RE.match(line.strip()):
            continue
        next_line = lines[index + 1] if index + 1 < len(lines) else ""
        if header is None and SEPARATOR_RE.match(next_line.strip()):
            header = [_clean_cell(c) for c in _split_row(line)]
            continue
        if header is not None:
            yield header, _split_row(line), index + 1


def _skipped(rel: Path) -> bool:
    if any(part in SKIP_PARTS for part in rel.parts):
        return True
    return any(rel.is_relative_to(d) for d in SKIP_DIRS)


def _level(rel: Path) -> str:
    parts = rel.parts
    if parts[:2] == ("hil", "tests"):
        return "HIL"
    if "integration_test" in parts or ("test" in parts and "integration" in rel.name):
        return "IT"
    return "UT"


def scan_tags(root: Path) -> tuple[list[Tag], list[Violation]]:
    """Collect code tags under the source and test directories of ``root``."""
    tags: list[Tag] = []
    problems: list[Violation] = []
    for top in SCAN_ROOTS:
        base = root / top
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if path.suffix not in SOURCE_SUFFIXES or not path.is_file():
                continue
            rel = path.relative_to(root)
            if _skipped(rel):
                continue
            try:
                text = path.read_text(encoding="utf-8")
            except UnicodeDecodeError:
                continue
            for relation, ids, number in _file_tags(text, path.suffix):
                location = f"{rel.as_posix()}:{number}"
                for identifier in ids:
                    if not ID_RE.fullmatch(identifier):
                        problems.append(
                            Violation("T1", identifier, f"malformed identifier at {location}")
                        )
                        continue
                    level = "impl" if relation == "satisfies" else _level(rel)
                    tags.append(Tag(relation, identifier, rel.as_posix(), number, level))
    return tags, problems


def _file_tags(text: str, suffix: str) -> Iterator[tuple[str, list[str], int]]:
    for number, line in enumerate(text.splitlines(), start=1):
        for match in C_TAG_RE.finditer(line):
            yield match.group(1), ID_LIKE_RE.findall(match.group(2)), number
    if suffix == ".py":
        for match in PY_MARK_RE.finditer(text):
            values = [a or b for a, b in re.findall(r'"([^"]+)"|\'([^\']+)\'', match.group(2))]
            number = text.count("\n", 0, match.start()) + 1
            yield match.group(1), values, number


def read_junit(paths: Iterable[Path]) -> list[JunitResult]:
    """Read test cases with ``verifies`` or ``test_id`` properties from JUnit XML files."""
    results: list[JunitResult] = []
    for path in paths:
        for case in ET.parse(path).getroot().iter("testcase"):
            ids: list[str] = []
            test_id = ""
            for prop in case.iter("property"):
                value = prop.get("value", "")
                if prop.get("name") == "verifies":
                    ids.extend(_ids_in(value))
                elif prop.get("name") == "test_id":
                    test_id = value
            if not ids:
                continue
            failed = any(case.find(tag) is not None for tag in ("failure", "error", "skipped"))
            name = f"{case.get('classname', '')}::{case.get('name', '')}"
            results.append(JunitResult(name, tuple(ids), not failed, test_id.startswith("TST-HIL")))
    return results


@dataclass
class Matrix:
    """Joined view of items, tags and results."""

    items: dict[str, Item]
    tags: list[Tag]
    junit: list[JunitResult]
    tag_problems: list[Violation]

    def __post_init__(self) -> None:
        self.children: dict[str, set[str]] = defaultdict(set)
        for item in self.items.values():
            for parent in item.parents:
                self.children[parent].add(item.id)
        for sm in (i for i in self.items.values() if i.kind == "SM"):
            for implementer in sm.implemented_by:
                self.children[sm.id].add(implementer)
        self.satisfied: dict[str, list[Tag]] = defaultdict(list)
        self.verified: dict[str, list[Tag]] = defaultdict(list)
        for tag in self.tags:
            target = self.satisfied if tag.relation == "satisfies" else self.verified
            if tag.relation in {"satisfies", "verifies"}:
                target[tag.id].append(tag)
        self.results: dict[str, list[JunitResult]] = defaultdict(list)
        for result in self.junit:
            for identifier in result.ids:
                self.results[identifier].append(result)

    def tests_of(self, identifier: str) -> list[Tag]:
        """Verification tags of an identifier."""
        return self.verified.get(identifier, [])

    def hil_tests_of(self, identifier: str) -> list[Tag]:
        """HIL verification tags of an identifier."""
        return [t for t in self.tests_of(identifier) if t.level == "HIL"]


def evaluate(matrix: Matrix, release: bool = False) -> list[Violation]:
    """Evaluate rules T1-T7 and return every violation (blocking or not)."""
    items = matrix.items
    out: list[Violation] = list(matrix.tag_problems)
    for tag in matrix.tags:
        if tag.id not in items:
            out.append(Violation("T1", tag.id, f"unknown identifier at {tag.path}:{tag.line}"))
    mvp = [i for i in items.values() if i.is_mvp]
    for item in (i for i in mvp if i.kind == "SYS"):
        if not any(kind_of(c) in {"HWR"} or c.startswith("SWR-") for c in matrix.children[item.id]):
            out.append(Violation("T2", item.id, "no SWR or HWR satisfies it"))
        if ("HIL" in item.methods() or item.is_safety) and not matrix.hil_tests_of(item.id):
            out.append(Violation("T4", item.id, "no HIL test"))
    for item in (i for i in mvp if i.kind.startswith("SWR-")):
        rule = f"T3:{item.kind.split('-')[1]}"
        if not matrix.satisfied.get(item.id):
            out.append(Violation(rule, item.id, "no @satisfies location"))
        if not matrix.tests_of(item.id):
            out.append(Violation(rule, item.id, "no @verifies test"))
    for item in (i for i in mvp if i.kind == "SM"):
        implementers = {c for c in matrix.children[item.id] if c in items}
        if not implementers:
            out.append(Violation("T5", item.id, "no implementing requirement"))
        if not any(matrix.tests_of(x) for x in {item.id, *implementers}):
            out.append(Violation("T5", item.id, "no test"))
    for item in (i for i in items.values() if i.kind == "CSR"):
        verifiers = {item.id, *matrix.children[item.id]}
        procedures = [x for x in _ids_in(item.ver) if x.startswith("TST-") and x in items]
        if not procedures and not any(matrix.tests_of(x) for x in verifiers):
            out.append(Violation("T6", item.id, "no verification entry"))
    if release:
        for item in (i for i in mvp if i.kind == "SYS" or i.kind.startswith("SWR-")):
            results = matrix.results.get(item.id, [])
            if not results or not all(r.passed for r in results):
                out.append(Violation("T7", item.id, "no passing release evidence"))
            elif item.kind == "SYS" and item.is_safety and not any(r.hil for r in results):
                out.append(Violation("T7", item.id, "no passing HIL result"))
    return out


def blocking(violations: Iterable[Violation], phase: int, release: bool) -> list[Violation]:
    """Return the violations that block in ``phase``."""
    rules = set(PHASE_RULES[phase])
    if release:
        rules.add("T7")
    return [v for v in violations if v.rule in rules]


def _row(matrix: Matrix, item: Item, issues: dict[str, list[str]]) -> dict[str, Any]:
    tests = matrix.tests_of(item.id)
    results = matrix.results.get(item.id, [])
    return {
        "id": item.id,
        "kind": item.kind,
        "mvp": item.is_mvp,
        "safety": item.is_safety,
        "title": item.title,
        "doc": item.doc,
        "parents": sorted(item.parents),
        "children": sorted(matrix.children.get(item.id, set())),
        "ver": item.ver,
        "stage": item.stage,
        "milestone": item.milestone,
        "satisfied_by": [f"{t.path}:{t.line}" for t in matrix.satisfied.get(item.id, [])],
        "verified_by": [f"{t.path}:{t.line} ({t.level})" for t in tests],
        "junit": {
            "passed": sum(r.passed for r in results),
            "failed": sum(not r.passed for r in results),
        },
        "issues": issues.get(item.id, []),
    }


def write_outputs(
    matrix: Matrix, violations: list[Violation], blocked: list[Violation], out_dir: Path
) -> str:
    """Write the CSV, JSON and Markdown matrices and return the Markdown summary."""
    out_dir.mkdir(parents=True, exist_ok=True)
    issues: dict[str, list[str]] = defaultdict(list)
    for v in violations:
        issues[v.id].append(f"{v.rule}: {v.message}")
    rows = [_row(matrix, matrix.items[k], issues) for k in sorted(matrix.items)]
    (out_dir / "trace_matrix.json").write_text(
        json.dumps(
            {
                "items": rows,
                "violations": [v.__dict__ for v in violations],
                "blocking": [v.__dict__ for v in blocked],
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    with (out_dir / "trace_matrix.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(
            [
                "id",
                "kind",
                "mvp",
                "safety",
                "parents",
                "children",
                "satisfied",
                "verified",
                "issues",
            ]
        )
        for r in rows:
            writer.writerow(
                [
                    r["id"],
                    r["kind"],
                    r["mvp"],
                    r["safety"],
                    " ".join(r["parents"]),
                    " ".join(r["children"]),
                    len(r["satisfied_by"]),
                    len(r["verified_by"]),
                    "; ".join(r["issues"]),
                ]
            )
    summary = _summary(matrix, violations, blocked)
    lines = [
        summary,
        "",
        "| ID | MVP | Parents | Satisfied | Verified | Issues |",
        "|---|---|---|---|---|---|",
    ]
    for r in rows:
        lines.append(
            f"| {r['id']} | {'yes' if r['mvp'] else 'no'} | {', '.join(r['parents'])} | "
            f"{len(r['satisfied_by'])} | {len(r['verified_by'])} | {'; '.join(r['issues'])} |"
        )
    (out_dir / "trace_matrix.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    return summary


def _summary(matrix: Matrix, violations: list[Violation], blocked: list[Violation]) -> str:
    kinds: dict[str, int] = defaultdict(int)
    for item in matrix.items.values():
        kinds[item.kind] += 1
    per_rule: dict[str, int] = defaultdict(int)
    for v in violations:
        per_rule[v.rule] += 1
    lines = [
        "## Traceability",
        "",
        f"Identifiers: {len(matrix.items)} ("
        + ", ".join(f"{k} {n}" for k, n in sorted(kinds.items()))
        + f"); code tags: {len(matrix.tags)}; JUnit cases: {len(matrix.junit)}.",
        "",
        "| Rule | Findings |",
        "|---|---|",
    ]
    lines += [f"| {rule} | {count} |" for rule, count in sorted(per_rule.items())]
    lines += ["", f"Blocking findings: {len(blocked)}."]
    return "\n".join(lines)


def build(root: Path, junit: Sequence[Path]) -> Matrix:
    """Parse the repository and join the sources."""
    items = parse_docs(root / "docs", root)
    tags, problems = scan_tags(root)
    return Matrix(items, tags, read_junit(junit), problems)


def parse_args(argv: Sequence[str] | None) -> argparse.Namespace:
    """Parse the command line."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--report", action="store_true", help="write the matrix; never fails")
    mode.add_argument("--gate", action="store_true", help="fail on blocking violations")
    parser.add_argument("--phase", type=int, choices=sorted(PHASE_RULES), default=4)
    parser.add_argument("--release", action="store_true", help="add rule T7 (release branches)")
    parser.add_argument("--junit", type=Path, nargs="*", default=[], help="JUnit XML files")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--out-dir", type=Path, default=None, help="default: <root>/build/trace")
    parser.add_argument("-v", "--verbose", action="store_true", help="list every finding")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    """Run the report or the gate."""
    args = parse_args(argv)
    out_dir = args.out_dir or args.root / "build" / "trace"
    try:
        matrix = build(args.root, args.junit)
        violations = evaluate(matrix, release=args.release)
        blocked = blocking(violations, args.phase, args.release) if args.gate else []
        summary = write_outputs(matrix, violations, blocked, out_dir)
    except Exception as exc:
        # Report mode never fails; the gate fails closed on input errors only.
        if not args.report and not isinstance(exc, OSError | ET.ParseError | ValueError):
            raise
        print(f"trace: {exc}", file=sys.stderr)
        return 0 if args.report else 2
    print(summary)
    print(f"\nMatrix written to {out_dir}")
    for v in violations if args.verbose else blocked:
        print(f"{v.rule} {v.id}: {v.message}")
    step_summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if step_summary:
        with Path(step_summary).open("a", encoding="utf-8") as stream:
            stream.write(summary + "\n")
    if args.report:
        return 0
    return 1 if blocked else 0


if __name__ == "__main__":
    sys.exit(main())
