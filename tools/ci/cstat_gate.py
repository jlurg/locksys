# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
# /// script
# requires-python = ">=3.13"
# dependencies = ["pyyaml==6.0.3"]
# ///
"""C-STAT quality gate for the IAR build of the DCU.

Each check runs when its inputs are given:

* CS001 ``--sarif``: no unsuppressed C-STAT result in the SARIF 2.1.0 output.
* CS002 ``--sources``/``--deviations``: every C-STAT suppression directive
  (``/*cstat -tag ...*/``, ``!tag``, ``#tag``, ``#pragma cstat_disable``,
  ``#pragma cstat_suppress``) cites an approved deviation record whose
  ``cstat_tags`` cover the suppressed check; none appears in generated code.
* CS003 ``--sources``/``--deviations``: every approved in-scope deviation
  record is referenced by at least one directive.
* CS004 ``--map``/``--release-map``: no forbidden symbol in the linker map
  (heap functions in every configuration; fault-injection symbols in Release).
* CS005 ``--compiler-version[-file]``: the compiler version matches the pin in
  ``tools/versions.env``.

``--sarif-out`` writes one merged SARIF run with repository-relative URIs for
code-scanning upload. Exit status: 0 = pass, 1 = gate failure, 2 = input error.
"""

from __future__ import annotations

import argparse
import copy
import fnmatch
import json
import posixpath
import re
import sys
import urllib.parse
from collections import Counter
from collections.abc import Iterable, Iterator, Mapping, Sequence
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import yaml

EXIT_PASS = 0
EXIT_FAIL = 1
EXIT_INPUT_ERROR = 2

DEFAULT_CONFIG = Path(__file__).with_name("quality_gates.yaml")
DEFAULT_SETTINGS: dict[str, Any] = {
    "deviation_id_pattern": r"DEV-(?:DCU|CGW|LIB)-[0-9]{3}|DP-[0-9]{2}",
    "referenced_prefixes": ["DEV-DCU-", "DEV-LIB-"],
    "approved_statuses": ["approved"],
    "generated_path_pattern": r"(^|/)gen(_vs)?/",
    "source_extensions": [".c", ".h"],
    "compiler_version_key": "IAR_EWARM_BASELINE",
    "forbidden_symbols": {
        "all": [r"^(malloc|calloc|realloc|free)$"],
        "release": [r"^Fi_", r"^FaultInj_"],
    },
}
MAX_LISTED = 50

_BLOCK_COMMENT = re.compile(r"/\*(.*?)\*/", re.DOTALL)
_LINE_COMMENT = re.compile(r"//([^\n]*)")
_DIRECTIVE_HEAD = re.compile(r"^\s*cstat(?=[\s:]|$)(.*)$", re.DOTALL)
_DIRECTIVE_OP = re.compile(r"^([+\-!#])([A-Za-z0-9_.*\-]+)$")
_PRAGMA = re.compile(r"^\s*#\s*pragma\s+cstat_(disable|enable|restore|suppress)\b(.*)$")
_PRAGMA_TAG = re.compile(r'"([^"]*)"')
_COMPILER_VERSION = re.compile(r"\bV(\d+(?:\.\d+){1,3})\b")
_ENTRY_LIST_HEADING = re.compile(r"^\*\*\* ENTRY LIST\s*$")
_MAP_HEADING = re.compile(r"^\*\*\* \S")
_IDENTIFIER = re.compile(r"[A-Za-z_?$.][A-Za-z0-9_?$.]*")
_SUPPRESSING_OPS = {"-", "!", "#"}
_SUPPRESSING_PRAGMAS = {"disable", "suppress"}


class GateInputError(Exception):
    """Raised when an input file is missing or malformed."""


@dataclass
class CheckResult:
    """Outcome of one gate check."""

    code: str
    title: str
    errors: list[str] = field(default_factory=list)
    notes: list[str] = field(default_factory=list)

    @property
    def passed(self) -> bool:
        """True when the check reported no error."""
        return not self.errors


@dataclass(frozen=True)
class Finding:
    """One C-STAT result taken from SARIF."""

    rule: str
    path: str
    line: int | None
    message: str
    suppressed: bool

    def location(self) -> str:
        """Return ``path:line`` (or ``path`` when the line is unknown)."""
        return f"{self.path}:{self.line}" if self.line is not None else self.path


@dataclass(frozen=True)
class Suppression:
    """One C-STAT suppression found in the sources."""

    path: str
    line: int
    tag: str
    deviation_ids: tuple[str, ...]
    origin: str


@dataclass(frozen=True)
class DeviationRecord:
    """A deviation or permit record from the registry."""

    record_id: str
    status: str
    tags: tuple[str, ...]


# --------------------------------------------------------------------------- configuration


def load_settings(config_path: Path | None) -> dict[str, Any]:
    """Return the ``cstat`` section of the quality-gate configuration merged over defaults."""
    settings = copy.deepcopy(DEFAULT_SETTINGS)
    if config_path is None:
        return settings
    try:
        document = yaml.safe_load(config_path.read_text(encoding="utf-8")) or {}
    except (OSError, yaml.YAMLError) as exc:
        raise GateInputError(f"{config_path}: cannot read configuration: {exc}") from exc
    section = document.get("cstat", {}) if isinstance(document, Mapping) else {}
    if not isinstance(section, Mapping):
        raise GateInputError(f"{config_path}: 'cstat' must be a mapping")
    for key, value in section.items():
        if key == "forbidden_symbols":
            if not isinstance(value, Mapping):
                raise GateInputError(f"{config_path}: 'cstat.forbidden_symbols' must be a mapping")
            settings["forbidden_symbols"] = {str(k): list(v) for k, v in value.items()}
        else:
            settings[str(key)] = value
    return settings


def parse_env_file(path: Path) -> dict[str, str]:
    """Parse a ``KEY=VALUE`` file such as ``tools/versions.env``."""
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as exc:
        raise GateInputError(f"{path}: cannot read: {exc}") from exc
    values: dict[str, str] = {}
    for raw in lines:
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("export "):
            line = line[len("export ") :].lstrip()
        key, sep, value = line.partition("=")
        if not sep:
            continue
        value = value.strip()
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]
        else:
            value = re.split(r"\s+#", value, maxsplit=1)[0].strip()
        values[key.strip()] = value
    return values


# --------------------------------------------------------------------------- SARIF


def _expand_sarif_inputs(paths: Iterable[Path]) -> list[Path]:
    files: list[Path] = []
    for path in paths:
        if path.is_dir():
            files.extend(sorted(path.rglob("*.sarif")))
        elif path.is_file():
            files.append(path)
        else:
            raise GateInputError(f"{path}: SARIF input not found")
    return files


def read_sarif(path: Path) -> Mapping[str, Any]:
    try:
        document = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError) as exc:
        raise GateInputError(f"{path}: cannot parse SARIF: {exc}") from exc
    if not isinstance(document, Mapping) or not isinstance(document.get("runs"), list):
        raise GateInputError(f"{path}: not a SARIF log (missing 'runs')")
    return document


def _path_from_uri(uri: str) -> str:
    if uri.lower().startswith("file:"):
        parsed = urllib.parse.urlparse(uri)
        path = urllib.parse.unquote(parsed.path)
        if parsed.netloc and parsed.netloc.lower() != "localhost":
            path = f"//{parsed.netloc}{path}"
        if re.match(r"^/[A-Za-z]:", path):
            path = path[1:]
        return path.replace("\\", "/")
    return urllib.parse.unquote(uri).replace("\\", "/")


def _is_absolute(path: str) -> bool:
    return path.startswith("/") or re.match(r"^[A-Za-z]:/", path) is not None


def relativize(uri: str, source_root: str, base_uri: str | None = None) -> str:
    """Return a repository-relative POSIX path for a SARIF artifact URI when possible.

    Windows paths are compared case-insensitively. URIs outside the source root
    are returned as absolute paths.
    """
    path = _path_from_uri(uri)
    while path.startswith("./"):
        path = path[2:]
    if not _is_absolute(path) and base_uri:
        path = _path_from_uri(base_uri).rstrip("/") + "/" + path
    if not _is_absolute(path):
        return posixpath.normpath(path)
    path = posixpath.normpath(path)
    root = posixpath.normpath(source_root.replace("\\", "/")).rstrip("/") + "/"
    if path.lower().startswith(root.lower()):
        return path[len(root) :]
    return path


def _base_uris(run: Mapping[str, Any]) -> dict[str, str]:
    bases: dict[str, str] = {}
    original = run.get("originalUriBaseIds")
    if isinstance(original, Mapping):
        for name, value in original.items():
            if isinstance(value, Mapping) and isinstance(value.get("uri"), str):
                bases[str(name)] = value["uri"]
    return bases


def _rule_id(result: Mapping[str, Any], rules: Sequence[Any]) -> str:
    rule_id = result.get("ruleId")
    if isinstance(rule_id, str) and rule_id:
        return rule_id
    rule = result.get("rule")
    if isinstance(rule, Mapping) and isinstance(rule.get("id"), str):
        return str(rule["id"])
    index = result.get("ruleIndex")
    if isinstance(index, int) and 0 <= index < len(rules):
        candidate = rules[index]
        if isinstance(candidate, Mapping) and isinstance(candidate.get("id"), str):
            return str(candidate["id"])
    return "unknown-rule"


def is_suppressed(result: Mapping[str, Any]) -> bool:
    """Apply SARIF 2.1.0 suppression semantics conservatively.

    A result is suppressed when its ``suppressions`` array is non-empty and no
    entry has status ``rejected`` or ``underReview`` (an absent status counts as
    ``accepted``).
    """
    suppressions = result.get("suppressions")
    if not isinstance(suppressions, list) or not suppressions:
        return False
    for entry in suppressions:
        status = entry.get("status", "accepted") if isinstance(entry, Mapping) else "rejected"
        if status != "accepted":
            return False
    return True


def _is_finding(result: Mapping[str, Any]) -> bool:
    if result.get("kind", "fail") != "fail":
        return False
    return result.get("baselineState") != "absent"


def _message(result: Mapping[str, Any]) -> str:
    message = result.get("message")
    if isinstance(message, Mapping) and isinstance(message.get("text"), str):
        return " ".join(message["text"].split())
    return ""


def _primary_location(
    result: Mapping[str, Any], bases: Mapping[str, str], root: str
) -> tuple[str, int | None]:
    locations = result.get("locations")
    if not isinstance(locations, list) or not locations:
        return "<no location>", None
    physical = locations[0].get("physicalLocation") if isinstance(locations[0], Mapping) else None
    if not isinstance(physical, Mapping):
        return "<no location>", None
    artifact = physical.get("artifactLocation")
    path = "<no location>"
    if isinstance(artifact, Mapping) and isinstance(artifact.get("uri"), str):
        base = bases.get(str(artifact.get("uriBaseId", "")))
        path = relativize(artifact["uri"], root, base)
    region = physical.get("region")
    line = region.get("startLine") if isinstance(region, Mapping) else None
    return path, line if isinstance(line, int) else None


def collect_findings(documents: Sequence[Mapping[str, Any]], source_root: str) -> list[Finding]:
    """Extract every failing C-STAT result from SARIF logs."""
    findings: list[Finding] = []
    for document in documents:
        for run in document["runs"]:
            if not isinstance(run, Mapping):
                continue
            rules = run.get("tool", {}).get("driver", {}).get("rules", [])
            rules = rules if isinstance(rules, list) else []
            bases = _base_uris(run)
            for result in run.get("results") or []:
                if not isinstance(result, Mapping) or not _is_finding(result):
                    continue
                path, line = _primary_location(result, bases, source_root)
                findings.append(
                    Finding(
                        rule=_rule_id(result, rules),
                        path=path,
                        line=line,
                        message=_message(result),
                        suppressed=is_suppressed(result),
                    )
                )
    return findings


def _normalize_locations(node: Any, bases: Mapping[str, str], root: str) -> None:
    if isinstance(node, dict):
        artifact = node.get("artifactLocation")
        if isinstance(artifact, dict) and isinstance(artifact.get("uri"), str):
            base = bases.get(str(artifact.get("uriBaseId", "")))
            artifact["uri"] = relativize(artifact["uri"], root, base)
            artifact.pop("uriBaseId", None)
            artifact.pop("index", None)
        for value in node.values():
            _normalize_locations(value, bases, root)
    elif isinstance(node, list):
        for item in node:
            _normalize_locations(item, bases, root)


def merge_sarif(documents: Sequence[Mapping[str, Any]], source_root: str) -> dict[str, Any]:
    """Merge all runs into one run with repository-relative artifact URIs."""
    driver: dict[str, Any] = {"name": "C-STAT"}
    merged_rules: list[dict[str, Any]] = []
    rule_index: dict[str, int] = {}
    results: list[dict[str, Any]] = []
    first_driver_seen = False
    for document in documents:
        for run in document["runs"]:
            if not isinstance(run, Mapping):
                continue
            run_driver = run.get("tool", {}).get("driver", {})
            if isinstance(run_driver, Mapping) and not first_driver_seen:
                first_driver_seen = True
                for key in ("name", "version", "semanticVersion", "informationUri", "organization"):
                    if isinstance(run_driver.get(key), str):
                        driver[key] = run_driver[key]
            rules = run_driver.get("rules", []) if isinstance(run_driver, Mapping) else []
            rules = rules if isinstance(rules, list) else []
            for rule in rules:
                if (
                    isinstance(rule, Mapping)
                    and isinstance(rule.get("id"), str)
                    and rule["id"] not in rule_index
                ):
                    rule_index[rule["id"]] = len(merged_rules)
                    merged_rules.append(copy.deepcopy(dict(rule)))
            bases = _base_uris(run)
            for result in run.get("results") or []:
                if not isinstance(result, Mapping):
                    continue
                item = copy.deepcopy(dict(result))
                rule_id = _rule_id(result, rules)
                item["ruleId"] = rule_id
                item.pop("rule", None)
                if rule_id in rule_index:
                    item["ruleIndex"] = rule_index[rule_id]
                else:
                    item.pop("ruleIndex", None)
                _normalize_locations(item, bases, source_root)
                results.append(item)
    driver["rules"] = merged_rules
    return {
        "$schema": "https://json.schemastore.org/sarif-2.1.0.json",
        "version": "2.1.0",
        "runs": [{"tool": {"driver": driver}, "results": results}],
    }


def check_sarif(findings: Sequence[Finding], file_count: int) -> CheckResult:
    """CS001: no unsuppressed result."""
    result = CheckResult("CS001", "Unsuppressed C-STAT results")
    if file_count == 0:
        result.errors.append("no SARIF file found; the analysis did not produce output")
        return result
    open_findings = [finding for finding in findings if not finding.suppressed]
    suppressed = len(findings) - len(open_findings)
    result.notes.append(
        f"{file_count} SARIF file(s): {len(open_findings)} open, {suppressed} suppressed result(s)"
    )
    for rule, count in sorted(Counter(f.rule for f in open_findings).items()):
        result.notes.append(f"{rule}: {count}")
    for finding in open_findings[:MAX_LISTED]:
        result.errors.append(f"{finding.location()}: [{finding.rule}] {finding.message}")
    if len(open_findings) > MAX_LISTED:
        result.errors.append(f"... {len(open_findings) - MAX_LISTED} more open result(s)")
    return result


# --------------------------------------------------------------------------- deviations


def load_deviations(path: Path) -> dict[str, DeviationRecord]:
    """Load the deviation registry (a list, or a mapping of record lists)."""
    try:
        document = yaml.safe_load(path.read_text(encoding="utf-8"))
    except (OSError, yaml.YAMLError) as exc:
        raise GateInputError(f"{path}: cannot read deviation registry: {exc}") from exc
    if document is None:
        raw_records: list[Any] = []
    elif isinstance(document, list):
        raw_records = document
    elif isinstance(document, Mapping):
        lists = [document[key] for key in ("deviations", "permits", "records") if key in document]
        if not lists or not all(isinstance(item, list) for item in lists):
            raise GateInputError(
                f"{path}: expected record lists under 'deviations', 'permits' or 'records'"
            )
        raw_records = [record for item in lists for record in item]
    else:
        raise GateInputError(f"{path}: unsupported registry format")
    records: dict[str, DeviationRecord] = {}
    for raw in raw_records:
        if not isinstance(raw, Mapping) or not isinstance(raw.get("id"), str):
            raise GateInputError(f"{path}: every record needs a string 'id'")
        record_id = raw["id"].strip()
        if record_id in records:
            raise GateInputError(f"{path}: duplicate record id {record_id}")
        tags_value = raw.get("cstat_tags", raw.get("cstat_tag", []))
        if isinstance(tags_value, str):
            tags: tuple[str, ...] = (tags_value,)
        elif isinstance(tags_value, list) and all(isinstance(tag, str) for tag in tags_value):
            tags = tuple(tags_value)
        else:
            raise GateInputError(
                f"{path}: {record_id}: 'cstat_tags' must be a string or a list of strings"
            )
        records[record_id] = DeviationRecord(
            record_id=record_id,
            status=str(raw.get("status", "")).strip().lower(),
            tags=tags,
        )
    return records


def _logical_lines(text: str) -> Iterator[tuple[int, str]]:
    """Yield (first line number, text) with backslash continuations joined."""
    pending: list[str] = []
    start = 0
    for number, line in enumerate(text.splitlines(), start=1):
        if not pending:
            start = number
        if line.endswith("\\"):
            pending.append(line[:-1])
            continue
        pending.append(line)
        yield start, " ".join(pending)
        pending = []
    if pending:
        yield start, " ".join(pending)


def _comment_spans(text: str) -> list[tuple[int, str]]:
    """Return (line number, body) for every block and line comment."""
    spans: list[tuple[int, int, str]] = []
    for match in _BLOCK_COMMENT.finditer(text):
        spans.append((match.start(), match.end(), match.group(1)))
    for match in _LINE_COMMENT.finditer(text):
        if any(start <= match.start() < end for start, end, _ in spans):
            continue
        spans.append((match.start(), match.end(), match.group(1)))
    spans.sort()
    return [(text.count("\n", 0, start) + 1, body) for start, _, body in spans]


def scan_sources(
    paths: Sequence[Path], extensions: Sequence[str], id_pattern: re.Pattern[str], repo_root: Path
) -> tuple[list[Suppression], list[str]]:
    """Find C-STAT suppressions; return them and syntax errors."""
    suppressions: list[Suppression] = []
    errors: list[str] = []
    files: list[Path] = []
    for path in paths:
        if path.is_dir():
            files.extend(
                sorted(p for p in path.rglob("*") if p.is_file() and p.suffix in extensions)
            )
        elif path.is_file():
            files.append(path)
        else:
            raise GateInputError(f"{path}: source path not found")
    for file in files:
        try:
            text = file.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            raise GateInputError(f"{file}: cannot read: {exc}") from exc
        display = _display_path(file, repo_root)
        for line, body in _comment_spans(text):
            head = _DIRECTIVE_HEAD.match(body)
            if head is None:
                continue
            operations, _, justification = head.group(1).partition(":")
            ids = tuple(id_pattern.findall(justification))
            for token in operations.split():
                op = _DIRECTIVE_OP.match(token)
                if op is None:
                    errors.append(f"{display}:{line}: malformed C-STAT directive token '{token}'")
                    continue
                if op.group(1) in _SUPPRESSING_OPS:
                    suppressions.append(
                        Suppression(display, line, op.group(2), ids, f"cstat {token}")
                    )
        lines = text.splitlines()
        for line, logical in _logical_lines(text):
            pragma = _PRAGMA.match(logical)
            if pragma is None or pragma.group(1) not in _SUPPRESSING_PRAGMAS:
                continue
            tags = _PRAGMA_TAG.findall(pragma.group(2))
            if not tags:
                errors.append(
                    f"{display}:{line}: cstat_{pragma.group(1)} pragma without a quoted check tag"
                )
                continue
            context = logical if line < 2 else lines[line - 2] + "\n" + logical
            ids = tuple(id_pattern.findall(context))
            for tag in tags:
                suppressions.append(
                    Suppression(display, line, tag, ids, f"pragma cstat_{pragma.group(1)}")
                )
    return suppressions, errors


def _display_path(path: Path, repo_root: Path) -> str:
    try:
        return path.resolve().relative_to(repo_root.resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def check_suppressions(
    suppressions: Sequence[Suppression],
    syntax_errors: Sequence[str],
    records: Mapping[str, DeviationRecord],
    settings: Mapping[str, Any],
) -> tuple[CheckResult, CheckResult]:
    """CS002 (directives justified) and CS003 (approved records referenced)."""
    justified = CheckResult("CS002", "Suppressions cite approved deviations")
    referenced = CheckResult("CS003", "Approved deviations are referenced")
    justified.errors.extend(syntax_errors)
    approved = {status.lower() for status in settings["approved_statuses"]}
    generated = re.compile(settings["generated_path_pattern"])
    used: set[str] = set()
    for item in suppressions:
        where = f"{item.path}:{item.line}: {item.origin}"
        if generated.search(item.path):
            justified.errors.append(f"{where}: suppressions are not allowed in generated code")
            continue
        if not item.deviation_ids:
            justified.errors.append(f"{where}: no deviation id in the justification")
            continue
        covered = False
        for record_id in item.deviation_ids:
            record = records.get(record_id)
            if record is None:
                justified.errors.append(f"{where}: unknown deviation {record_id}")
                continue
            used.add(record_id)
            if record.status not in approved:
                justified.errors.append(
                    f"{where}: deviation {record_id} has status '{record.status or 'unset'}'"
                )
                continue
            if not record.tags:
                justified.errors.append(f"{where}: deviation {record_id} lists no cstat_tags")
                continue
            if any(fnmatch.fnmatchcase(item.tag, pattern) for pattern in record.tags):
                covered = True
            else:
                justified.errors.append(f"{where}: deviation {record_id} does not cover {item.tag}")
        if covered:
            justified.notes.append(f"{where}: {item.tag} -> {', '.join(item.deviation_ids)}")
    prefixes = tuple(settings["referenced_prefixes"])
    for record in sorted(records.values(), key=lambda r: r.record_id):
        if (
            record.status in approved
            and record.record_id.startswith(prefixes)
            and record.record_id not in used
        ):
            referenced.errors.append(
                f"{record.record_id}: approved but not referenced by any suppression"
            )
    justified.notes.insert(0, f"{len(suppressions)} suppression(s) found")
    return justified, referenced


# --------------------------------------------------------------------------- map files and toolchain


def map_symbols(text: str) -> set[str]:
    """Return symbol names from an IAR ILINK map ENTRY LIST, or all identifiers otherwise."""
    lines = text.splitlines()
    start = next((i for i, line in enumerate(lines) if _ENTRY_LIST_HEADING.match(line)), None)
    if start is None:
        return set(_IDENTIFIER.findall(text))
    symbols: set[str] = set()
    for line in lines[start + 1 :]:
        if _MAP_HEADING.match(line):
            break
        stripped = line.strip()
        if not stripped:
            continue
        first = stripped.split()[0]
        if _IDENTIFIER.fullmatch(first):
            symbols.add(first)
    return symbols


def check_maps(
    maps: Sequence[Path], release_maps: Sequence[Path], settings: Mapping[str, Any]
) -> CheckResult:
    """CS004: no forbidden symbol in the linker maps."""
    result = CheckResult("CS004", "Forbidden symbols in linker maps")
    forbidden = settings["forbidden_symbols"]
    common = [re.compile(p) for p in forbidden.get("all", [])]
    release_only = [re.compile(p) for p in forbidden.get("release", [])]
    jobs = [(path, common) for path in maps] + [
        (path, common + release_only) for path in release_maps
    ]
    for path, patterns in jobs:
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            raise GateInputError(f"{path}: cannot read map file: {exc}") from exc
        hits = sorted(s for s in map_symbols(text) if any(p.search(s) for p in patterns))
        if hits:
            result.errors.append(f"{path.as_posix()}: forbidden symbol(s): {', '.join(hits)}")
        else:
            result.notes.append(f"{path.as_posix()}: clean ({len(patterns)} pattern(s))")
    return result


def check_compiler(version_text: str, pins: Mapping[str, str], key: str) -> CheckResult:
    """CS005: compiler version matches the pinned EWARM version (dotted prefix match)."""
    result = CheckResult("CS005", "Compiler version matches tools/versions.env")
    expected = pins.get(key, "").strip()
    if not expected:
        result.errors.append(f"pin {key} missing in versions file")
        return result
    match = _COMPILER_VERSION.search(version_text)
    if match is None:
        result.errors.append(f"no version found in compiler output: {version_text.strip()[:120]!r}")
        return result
    actual = match.group(1)
    expected_parts = expected.split(".")
    if actual.split(".")[: len(expected_parts)] != expected_parts:
        result.errors.append(f"compiler V{actual} does not match {key}={expected}")
    else:
        result.notes.append(f"compiler V{actual} matches {key}={expected}")
    return result


# --------------------------------------------------------------------------- reporting and CLI


def render_summary(results: Sequence[CheckResult]) -> str:
    """Return a Markdown summary of all checks."""
    lines = ["## C-STAT gate", "", "| Check | Result | Details |", "|---|---|---|"]
    for result in results:
        status = "pass" if result.passed else f"FAIL ({len(result.errors)})"
        detail = "; ".join(result.notes[:3]).replace("|", "\\|")
        lines.append(f"| {result.code} {result.title} | {status} | {detail} |")
    for result in results:
        if result.errors:
            lines += ["", f"### {result.code} errors", ""]
            lines += [f"- `{error}`" for error in result.errors[:MAX_LISTED]]
    return "\n".join(lines) + "\n"


def _parse_args(argv: Sequence[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--config", type=Path, default=DEFAULT_CONFIG, help="quality gate configuration"
    )
    parser.add_argument(
        "--sarif",
        type=Path,
        nargs="+",
        action="extend",
        default=[],
        help="SARIF files or directories",
    )
    parser.add_argument(
        "--sources",
        type=Path,
        nargs="+",
        action="extend",
        default=[],
        help="source files or directories",
    )
    parser.add_argument("--deviations", type=Path, help="deviation registry (YAML)")
    parser.add_argument(
        "--map", type=Path, action="append", default=[], help="linker map (heap check)"
    )
    parser.add_argument(
        "--release-map",
        type=Path,
        action="append",
        default=[],
        help="Release linker map (heap and fault-injection check)",
    )
    parser.add_argument("--compiler-version", help="output of 'iccarm --version'")
    parser.add_argument(
        "--compiler-version-file", type=Path, help="file holding the output of 'iccarm --version'"
    )
    parser.add_argument(
        "--versions-env", type=Path, default=Path("tools/versions.env"), help="version pin file"
    )
    parser.add_argument(
        "--source-root", type=Path, default=Path.cwd(), help="repository root for path mapping"
    )
    parser.add_argument(
        "--sarif-out", type=Path, help="write merged SARIF with repository-relative URIs"
    )
    parser.add_argument("--summary", type=Path, help="append a Markdown summary to this file")
    return parser.parse_args(argv)


def run(args: argparse.Namespace) -> list[CheckResult]:
    """Run the requested checks."""
    config = args.config if args.config and args.config.is_file() else None
    settings = load_settings(config)
    root = args.source_root.resolve()
    root_text = str(args.source_root).replace("\\", "/")
    if not _is_absolute(root_text):
        root_text = root.as_posix()
    results: list[CheckResult] = []
    if args.sarif:
        files = _expand_sarif_inputs(args.sarif)
        documents = [read_sarif(path) for path in files]
        results.append(check_sarif(collect_findings(documents, root_text), len(files)))
        if args.sarif_out:
            args.sarif_out.parent.mkdir(parents=True, exist_ok=True)
            merged = merge_sarif(documents, root_text)
            args.sarif_out.write_text(json.dumps(merged, indent=2) + "\n", encoding="utf-8")
    if args.sources or args.deviations:
        if not (args.sources and args.deviations):
            raise GateInputError("--sources and --deviations must be given together")
        records = load_deviations(args.deviations)
        id_pattern = re.compile(settings["deviation_id_pattern"])
        suppressions, syntax_errors = scan_sources(
            args.sources, settings["source_extensions"], id_pattern, root
        )
        results.extend(check_suppressions(suppressions, syntax_errors, records, settings))
    if args.map or args.release_map:
        results.append(check_maps(args.map, args.release_map, settings))
    if args.compiler_version is not None or args.compiler_version_file is not None:
        if args.compiler_version_file is not None:
            try:
                version_text = args.compiler_version_file.read_text(
                    encoding="utf-8-sig", errors="replace"
                )
            except OSError as exc:
                raise GateInputError(f"{args.compiler_version_file}: cannot read: {exc}") from exc
        else:
            version_text = args.compiler_version
        pins = parse_env_file(args.versions_env)
        results.append(check_compiler(version_text, pins, settings["compiler_version_key"]))
    if not results:
        raise GateInputError("no check requested")
    return results


def main(argv: Sequence[str] | None = None) -> int:
    """Command-line entry point."""
    args = _parse_args(argv)
    try:
        results = run(args)
    except GateInputError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return EXIT_INPUT_ERROR
    for result in results:
        print(f"{result.code} {result.title}: {'pass' if result.passed else 'FAIL'}")
        for note in result.notes:
            print(f"  {note}")
        for error in result.errors:
            print(f"  error: {error}")
    if args.summary:
        with args.summary.open("a", encoding="utf-8") as handle:
            handle.write(render_summary(results))
    return EXIT_PASS if all(result.passed for result in results) else EXIT_FAIL


if __name__ == "__main__":
    sys.exit(main())
