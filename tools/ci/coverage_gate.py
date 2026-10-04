# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
# /// script
# requires-python = ">=3.13"
# dependencies = ["pyyaml==6.0.3"]
# ///
"""Coverage gate: compare coverage reports with the thresholds in quality_gates.yaml.

Report formats are detected from the content:

* gcovr JSON (``gcovr --json``; the Ceedling gcov plugin writes it as
  ``build/artifacts/gcov/gcovr/GcovCoverage.json``)
* gcovr JSON summary (``gcovr --json-summary``)
* LCOV tracefile (``flutter test --coverage``, ``gcovr --lcov``)

Report paths are mapped to repository-relative paths (``--strip-prefix``,
``--path-prefix``, ``--prefixed-report``, ``--root``). Each file is assigned to
the first scope of the selected component whose patterns match; thresholds
apply to the aggregate of a scope (or to every file when ``per_file`` is set).

Exit status: 0 = pass, 1 = enforced threshold missed or no data, 2 = input error.
"""

from __future__ import annotations

import argparse
import json
import posixpath
import re
import sys
from collections.abc import Iterable, Mapping, Sequence
from dataclasses import dataclass, field
from fractions import Fraction
from pathlib import Path
from typing import Any

import yaml

EXIT_PASS = 0
EXIT_FAIL = 1
EXIT_INPUT_ERROR = 2

DEFAULT_CONFIG = Path(__file__).with_name("quality_gates.yaml")
METRICS = ("line", "branch")


class CoverageInputError(Exception):
    """Raised when a report or the configuration cannot be used."""


@dataclass
class FileCoverage:
    """Coverage of one source file.

    Line and branch hit counts are kept when the report provides them so that
    the same file reported twice is merged exactly; summary reports only carry
    totals.
    """

    path: str
    lines: dict[int, int] | None = None
    branches: dict[tuple[Any, ...], int] | None = None
    line_totals: tuple[int, int] = (0, 0)
    branch_totals: tuple[int, int] = (0, 0)

    def totals(self, metric: str) -> tuple[int, int]:
        """Return (covered, total) for ``line`` or ``branch``."""
        if metric == "line":
            if self.lines is not None:
                return sum(1 for count in self.lines.values() if count > 0), len(self.lines)
            return self.line_totals
        if self.branches is not None:
            return sum(1 for count in self.branches.values() if count > 0), len(self.branches)
        return self.branch_totals

    def merge(self, other: FileCoverage) -> None:
        """Merge another report of the same file into this one."""
        if self.lines is not None and other.lines is not None:
            for line, count in other.lines.items():
                self.lines[line] = max(self.lines.get(line, 0), count)
        else:
            self.line_totals = _max_pair(self.totals("line"), other.totals("line"))
            self.lines = None
        if self.branches is not None and other.branches is not None:
            for key, count in other.branches.items():
                self.branches[key] = max(self.branches.get(key, 0), count)
        else:
            self.branch_totals = _max_pair(self.totals("branch"), other.totals("branch"))
            self.branches = None


def _max_pair(a: tuple[int, int], b: tuple[int, int]) -> tuple[int, int]:
    return max(a[0], b[0]), max(a[1], b[1])


# --------------------------------------------------------------------------- report parsing


def _parse_gcovr_json(document: Mapping[str, Any]) -> list[FileCoverage]:
    files = []
    for entry in document.get("files", []):
        lines: dict[int, int] = {}
        branches: dict[tuple[Any, ...], int] = {}
        for line in entry.get("lines", []):
            if line.get("gcovr/excluded") or line.get("gcovr/noncode"):
                continue
            number = int(line["line_number"])
            lines[number] = max(lines.get(number, 0), int(line.get("count", 0)))
            for index, branch in enumerate(line.get("branches", [])):
                if branch.get("gcovr/excluded"):
                    continue
                key = (
                    number,
                    branch.get("source_block_id", branch.get("blockno")),
                    branch.get("branchno", index),
                )
                branches[key] = max(branches.get(key, 0), int(branch.get("count", 0)))
        files.append(FileCoverage(path=str(entry["file"]), lines=lines, branches=branches))
    return files


def _parse_gcovr_summary(document: Mapping[str, Any]) -> list[FileCoverage]:
    return [
        FileCoverage(
            path=str(entry["filename"]),
            line_totals=(int(entry.get("line_covered", 0)), int(entry.get("line_total", 0))),
            branch_totals=(int(entry.get("branch_covered", 0)), int(entry.get("branch_total", 0))),
        )
        for entry in document.get("files", [])
    ]


def _parse_lcov(text: str) -> list[FileCoverage]:
    files: list[FileCoverage] = []
    path: str | None = None
    lines: dict[int, int] = {}
    branches: dict[tuple[Any, ...], int] = {}
    for raw in text.splitlines():
        record = raw.strip()
        if record.startswith("SF:"):
            path, lines, branches = record[3:], {}, {}
        elif path is None:
            continue
        elif record.startswith("DA:"):
            fields = record[3:].split(",")
            number, count = int(fields[0]), int(fields[1])
            lines[number] = max(lines.get(number, 0), count)
        elif record.startswith("BRDA:"):
            number_text, block, branch, taken = record[5:].split(",")[:4]
            key = (int(number_text), block, branch)
            branches[key] = max(branches.get(key, 0), 0 if taken == "-" else int(taken))
        elif record == "end_of_record":
            files.append(FileCoverage(path=path, lines=lines, branches=branches))
            path = None
    if path is not None:
        files.append(FileCoverage(path=path, lines=lines, branches=branches))
    return files


def parse_report(path: Path) -> list[FileCoverage]:
    """Parse one coverage report, detecting its format."""
    try:
        text = path.read_text(encoding="utf-8-sig")
    except OSError as exc:
        raise CoverageInputError(f"{path}: cannot read report: {exc}") from exc
    try:
        if text.lstrip().startswith("{"):
            document = json.loads(text)
            files = document.get("files") if isinstance(document, Mapping) else None
            if not isinstance(files, list):
                raise CoverageInputError(f"{path}: JSON report without a 'files' list")
            if "gcovr/format_version" in document or (files and "lines" in files[0]):
                return _parse_gcovr_json(document)
            if "gcovr/summary_format_version" in document or (files and "line_total" in files[0]):
                return _parse_gcovr_summary(document)
            if not files:
                return []
            raise CoverageInputError(f"{path}: unrecognised JSON coverage format")
        if "SF:" in text:
            return _parse_lcov(text)
    except (ValueError, KeyError, TypeError) as exc:
        raise CoverageInputError(f"{path}: malformed report: {exc}") from exc
    raise CoverageInputError(f"{path}: unrecognised coverage format")


# --------------------------------------------------------------------------- path handling


def glob_to_regex(pattern: str) -> re.Pattern[str]:
    """Translate a path glob (``**``, ``*``, ``?``, ``{a,b}``) to an anchored regex."""
    out = []
    i = 0
    while i < len(pattern):
        char = pattern[i]
        if pattern.startswith("**/", i):
            out.append("(?:.*/)?")
            i += 3
        elif pattern.startswith("**", i):
            out.append(".*")
            i += 2
        elif char == "*":
            out.append("[^/]*")
            i += 1
        elif char == "?":
            out.append("[^/]")
            i += 1
        elif char == "{":
            end = pattern.find("}", i)
            if end == -1:
                raise CoverageInputError(f"unbalanced '{{' in pattern '{pattern}'")
            options = pattern[i + 1 : end].split(",")
            out.append("(?:" + "|".join(re.escape(option) for option in options) + ")")
            i = end + 1
        else:
            out.append(re.escape(char))
            i += 1
    return re.compile("^" + "".join(out) + "$")


def ceedling_report(project_dir: Path, repo_root: Path) -> tuple[Path, str]:
    """Return the gcovr JSON artifact of a Ceedling project and the prefix of its paths.

    Reads ``:project: :build_root:``, ``:gcov: :gcovr: :report_root:`` and
    ``:gcov: :gcovr: :json_artifact_filename:`` from ``project.yml``; Ceedling
    defaults apply when they are absent.
    """
    project_file = project_dir / "project.yml"
    try:
        document = yaml.safe_load(project_file.read_text(encoding="utf-8")) or {}
    except (OSError, yaml.YAMLError) as exc:
        raise CoverageInputError(f"{project_file}: cannot read Ceedling project: {exc}") from exc

    def lookup(*keys: str) -> Any:
        node: Any = document
        for key in keys:
            if not isinstance(node, Mapping):
                return None
            node = node.get(f":{key}", node.get(key))
        return node

    build_root = lookup("project", "build_root") or "build"
    report_root = lookup("gcov", "gcovr", "report_root") or "."
    file_name = lookup("gcov", "gcovr", "json_artifact_filename") or "GcovCoverage.json"
    report = project_dir / str(build_root) / "artifacts" / "gcov" / "gcovr" / str(file_name)
    prefix_dir = (project_dir / str(report_root)).resolve()
    try:
        prefix = prefix_dir.relative_to(repo_root.resolve()).as_posix()
    except ValueError as exc:
        raise CoverageInputError(
            f"{project_file}: gcovr report_root is outside the repository"
        ) from exc
    return report, "" if prefix == "." else prefix


def map_path(raw: str, root: str, strip_prefixes: Sequence[str], path_prefix: str) -> str:
    """Return a repository-relative POSIX path for a path found in a report."""
    path = raw.replace("\\", "/")
    for prefix in strip_prefixes:
        normalized = prefix.replace("\\", "/")
        if path.startswith(normalized):
            path = path[len(normalized) :].lstrip("/")
            break
    if path.startswith("/") or re.match(r"^[A-Za-z]:/", path):
        root_prefix = posixpath.normpath(root.replace("\\", "/")).rstrip("/") + "/"
        normalized_path = posixpath.normpath(path)
        if normalized_path.startswith(root_prefix):
            return normalized_path[len(root_prefix) :]
        return normalized_path
    if path_prefix:
        path = posixpath.join(path_prefix.replace("\\", "/"), path)
    return posixpath.normpath(path)


# --------------------------------------------------------------------------- configuration


@dataclass
class Scope:
    """A set of files sharing coverage thresholds."""

    name: str
    patterns: list[re.Pattern[str]]
    thresholds: dict[str, Fraction]
    enforce: bool
    per_file: bool
    required: bool
    files: list[FileCoverage] = field(default_factory=list)

    def matches(self, path: str) -> bool:
        """True when the path matches one of the scope patterns."""
        return any(pattern.match(path) for pattern in self.patterns)


@dataclass
class Component:
    """The scopes and exclusions of one component (dcu, libs, cgw, app, ...)."""

    name: str
    scopes: list[Scope]
    exclude: list[re.Pattern[str]]


def _as_fraction(value: Any, where: str) -> Fraction:
    if isinstance(value, bool) or not isinstance(value, int | float):
        raise CoverageInputError(f"{where}: threshold must be a number")
    fraction = Fraction(str(value))
    if not 0 <= fraction <= 100:
        raise CoverageInputError(f"{where}: threshold must be within 0..100")
    return fraction


def load_component(config_path: Path, name: str) -> Component:
    """Load and validate one component from the quality-gate configuration."""
    try:
        document = yaml.safe_load(config_path.read_text(encoding="utf-8")) or {}
    except (OSError, yaml.YAMLError) as exc:
        raise CoverageInputError(f"{config_path}: cannot read configuration: {exc}") from exc
    components = (
        document.get("coverage", {}).get("components", {}) if isinstance(document, Mapping) else {}
    )
    if not isinstance(components, Mapping) or name not in components:
        raise CoverageInputError(f"{config_path}: no coverage component '{name}'")
    raw = components[name]
    if not isinstance(raw, Mapping) or not isinstance(raw.get("scopes"), list) or not raw["scopes"]:
        raise CoverageInputError(
            f"{config_path}: component '{name}' needs a non-empty 'scopes' list"
        )
    component_enforce = bool(raw.get("enforce", True))
    scopes: list[Scope] = []
    for index, scope in enumerate(raw["scopes"]):
        where = f"{config_path}: coverage.components.{name}.scopes[{index}]"
        if not isinstance(scope, Mapping) or not isinstance(scope.get("name"), str):
            raise CoverageInputError(f"{where}: needs a 'name'")
        paths = scope.get("paths")
        if not isinstance(paths, list) or not paths or not all(isinstance(p, str) for p in paths):
            raise CoverageInputError(f"{where}: needs a non-empty 'paths' list")
        thresholds = {
            metric: _as_fraction(scope[metric], f"{where}.{metric}")
            for metric in METRICS
            if metric in scope
        }
        if not thresholds:
            raise CoverageInputError(f"{where}: needs at least one of {', '.join(METRICS)}")
        scopes.append(
            Scope(
                name=scope["name"],
                patterns=[glob_to_regex(p) for p in paths],
                thresholds=thresholds,
                enforce=bool(scope.get("enforce", component_enforce)),
                per_file=bool(scope.get("per_file", False)),
                required=bool(scope.get("required", False)),
            )
        )
    exclude = raw.get("exclude", [])
    if not isinstance(exclude, list) or not all(isinstance(p, str) for p in exclude):
        raise CoverageInputError(
            f"{config_path}: component '{name}': 'exclude' must be a list of globs"
        )
    return Component(name=name, scopes=scopes, exclude=[glob_to_regex(p) for p in exclude])


# --------------------------------------------------------------------------- evaluation


@dataclass
class Outcome:
    """Gate result: enforced failures, warnings and the per-scope table."""

    failures: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    rows: list[tuple[str, int, str, str]] = field(default_factory=list)
    unscoped: list[str] = field(default_factory=list)


def _percent(covered: int, total: int) -> str:
    if total == 0:
        return "n/a"
    return f"{covered * 100 / total:.1f}% ({covered}/{total})"


def _meets(covered: int, total: int, threshold: Fraction) -> bool:
    return total == 0 or covered * 100 >= threshold * total


def _aggregate(files: Iterable[FileCoverage], metric: str) -> tuple[int, int]:
    covered = total = 0
    for item in files:
        file_covered, file_total = item.totals(metric)
        covered += file_covered
        total += file_total
    return covered, total


def evaluate(component: Component, files: Iterable[FileCoverage], report_only: bool) -> Outcome:
    """Assign files to scopes and compare against the thresholds."""
    outcome = Outcome()
    for item in files:
        if any(pattern.match(item.path) for pattern in component.exclude):
            continue
        scope = next((s for s in component.scopes if s.matches(item.path)), None)
        if scope is None:
            outcome.unscoped.append(item.path)
        else:
            scope.files.append(item)
    for scope in component.scopes:
        enforce = scope.enforce and not report_only
        sink = outcome.failures if enforce else outcome.warnings
        if not scope.files:
            if scope.required:
                sink.append(f"{scope.name}: no coverage data for a required scope")
            outcome.rows.append((scope.name, 0, "n/a", "n/a"))
            continue
        line = _percent(*_aggregate(scope.files, "line"))
        branch = _percent(*_aggregate(scope.files, "branch"))
        outcome.rows.append((scope.name, len(scope.files), line, branch))
        for metric, threshold in scope.thresholds.items():
            units = (
                [(f"{scope.name} ({item.path})", [item]) for item in scope.files]
                if scope.per_file
                else [(scope.name, scope.files)]
            )
            for label, members in units:
                covered, total = _aggregate(members, metric)
                if not _meets(covered, total, threshold):
                    sink.append(
                        f"{label}: {metric} coverage {_percent(covered, total)} below {float(threshold):g}%"
                    )
    if not any(scope.files for scope in component.scopes):
        outcome.failures.append(
            f"component '{component.name}': no file of the reports falls in any scope"
        )
    return outcome


def render_summary(component: Component, outcome: Outcome) -> str:
    """Return a Markdown summary of the evaluation."""
    lines = [
        f"## Coverage gate: {component.name}",
        "",
        "| Scope | Files | Line | Branch |",
        "|---|---|---|---|",
    ]
    lines += [
        f"| {name} | {count} | {line} | {branch} |" for name, count, line, branch in outcome.rows
    ]
    for title, items in (
        ("Failures", outcome.failures),
        ("Warnings (not enforced)", outcome.warnings),
    ):
        if items:
            lines += ["", f"### {title}", ""] + [f"- {item}" for item in items]
    return "\n".join(lines) + "\n"


def main(argv: Sequence[str] | None = None) -> int:
    """Command-line entry point."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--config", type=Path, default=DEFAULT_CONFIG, help="quality gate configuration"
    )
    parser.add_argument(
        "--component", required=True, help="component name under coverage.components"
    )
    parser.add_argument(
        "--report", type=Path, action="append", default=[], help="coverage report (repeatable)"
    )
    parser.add_argument(
        "--ceedling-project",
        type=Path,
        action="append",
        default=[],
        help="Ceedling project directory; its gcovr JSON artifact is read (repeatable)",
    )
    parser.add_argument(
        "--root", default=str(Path.cwd()), help="repository root used to relativize paths"
    )
    parser.add_argument(
        "--strip-prefix", action="append", default=[], help="prefix removed from report paths"
    )
    parser.add_argument(
        "--path-prefix", default="", help="prefix prepended to relative paths of --report files"
    )
    parser.add_argument(
        "--prefixed-report",
        nargs=2,
        action="append",
        default=[],
        metavar=("PREFIX", "REPORT"),
        help="coverage report whose relative paths get their own prefix (repeatable)",
    )
    parser.add_argument("--report-only", action="store_true", help="never fail on thresholds")
    parser.add_argument("--summary", type=Path, help="append a Markdown summary to this file")
    args = parser.parse_args(argv)
    if not args.report and not args.ceedling_project and not args.prefixed_report:
        parser.error("at least one --report, --prefixed-report or --ceedling-project is required")
    try:
        component = load_component(args.config, args.component)
        inputs = [(report, args.path_prefix) for report in args.report]
        inputs += [(Path(report), prefix) for prefix, report in args.prefixed_report]
        inputs += [ceedling_report(project, Path(args.root)) for project in args.ceedling_project]
        merged: dict[str, FileCoverage] = {}
        for report, prefix in inputs:
            for item in parse_report(report):
                item.path = map_path(item.path, args.root, args.strip_prefix, prefix)
                if item.path in merged:
                    merged[item.path].merge(item)
                else:
                    merged[item.path] = item
    except CoverageInputError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return EXIT_INPUT_ERROR
    outcome = evaluate(component, merged.values(), args.report_only)
    for name, count, line, branch in outcome.rows:
        print(f"{name:<32} files={count:<4} line={line:<22} branch={branch}")
    if outcome.unscoped:
        print(f"{len(outcome.unscoped)} file(s) outside every scope (not gated):")
        for path in sorted(outcome.unscoped):
            print(f"  {path}")
    for warning in outcome.warnings:
        print(f"warning: {warning}")
    for failure in outcome.failures:
        print(f"error: {failure}")
    if args.summary:
        with args.summary.open("a", encoding="utf-8") as handle:
            handle.write(render_summary(component, outcome))
    return EXIT_FAIL if outcome.failures else EXIT_PASS


if __name__ == "__main__":
    sys.exit(main())
