# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
# /// script
# requires-python = ">=3.13"
# dependencies = ["lizard==1.24.0", "pyyaml==6.0.3"]
# ///
"""Complexity gate: apply the function-metric limits in quality_gates.yaml.

Runs lizard over the given paths (or reads previously generated
``lizard --csv -ENS`` output) and compares every function with the limits of
the ``complexity`` section: values above ``fail_above`` fail the gate, values
above ``warn_above`` are reported only.

Exit status: 0 = pass, 1 = limit exceeded, 2 = input error.
"""

from __future__ import annotations

import argparse
import csv
import io
import subprocess
import sys
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import yaml

EXIT_PASS = 0
EXIT_FAIL = 1
EXIT_INPUT_ERROR = 2

DEFAULT_CONFIG = Path(__file__).with_name("quality_gates.yaml")
CSV_COLUMNS = (
    "nloc",
    "ccn",
    "tokens",
    "parameters",
    "length",
    "location",
    "file",
    "function",
    "long_name",
)
NESTING_COLUMN = 11
METRIC_NAMES = ("ccn", "parameters", "nloc", "nesting")


class ComplexityInputError(Exception):
    """Raised when the configuration or the lizard output cannot be used."""


@dataclass(frozen=True)
class FunctionMetrics:
    """Metrics of one function as reported by lizard."""

    file: str
    function: str
    line: int
    values: Mapping[str, int]


@dataclass(frozen=True)
class Limit:
    """Warn and fail limits of one metric (None = not checked)."""

    warn_above: int | None
    fail_above: int | None


@dataclass(frozen=True)
class Settings:
    """The ``complexity`` section of the configuration."""

    paths: tuple[str, ...]
    exclude: tuple[str, ...]
    languages: tuple[str, ...]
    limits: Mapping[str, Limit]


def _optional_int(value: Any, where: str) -> int | None:
    if value is None:
        return None
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise ComplexityInputError(f"{where}: must be a non-negative integer")
    return value


def load_settings(config_path: Path) -> Settings:
    """Load and validate the ``complexity`` section."""
    try:
        document = yaml.safe_load(config_path.read_text(encoding="utf-8")) or {}
    except (OSError, yaml.YAMLError) as exc:
        raise ComplexityInputError(f"{config_path}: cannot read configuration: {exc}") from exc
    section = document.get("complexity") if isinstance(document, Mapping) else None
    if not isinstance(section, Mapping):
        raise ComplexityInputError(f"{config_path}: missing 'complexity' section")
    raw_limits = section.get("limits")
    if not isinstance(raw_limits, Mapping) or not raw_limits:
        raise ComplexityInputError(
            f"{config_path}: 'complexity.limits' must be a non-empty mapping"
        )
    limits: dict[str, Limit] = {}
    for name, raw in raw_limits.items():
        if name not in METRIC_NAMES or not isinstance(raw, Mapping):
            raise ComplexityInputError(f"{config_path}: unknown or malformed limit '{name}'")
        where = f"{config_path}: complexity.limits.{name}"
        limits[name] = Limit(
            warn_above=_optional_int(raw.get("warn_above"), f"{where}.warn_above"),
            fail_above=_optional_int(raw.get("fail_above"), f"{where}.fail_above"),
        )

    def _strings(key: str) -> tuple[str, ...]:
        value = section.get(key, [])
        if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
            raise ComplexityInputError(
                f"{config_path}: 'complexity.{key}' must be a list of strings"
            )
        return tuple(value)

    return Settings(
        paths=_strings("paths"),
        exclude=_strings("exclude"),
        languages=_strings("languages") or ("c",),
        limits=limits,
    )


def parse_lizard_csv(text: str) -> list[FunctionMetrics]:
    """Parse ``lizard --csv`` output (with or without the ``-ENS`` nesting column)."""
    functions: list[FunctionMetrics] = []
    for row in csv.reader(io.StringIO(text)):
        if not row:
            continue
        if row[0] == "NLOC":
            continue
        if len(row) < len(CSV_COLUMNS) + 2:
            raise ComplexityInputError(f"unexpected lizard CSV row: {row}")
        try:
            values = {
                "nloc": int(row[0]),
                "ccn": int(row[1]),
                "parameters": int(row[3]),
            }
            if len(row) > NESTING_COLUMN:
                values["nesting"] = int(row[NESTING_COLUMN])
            functions.append(
                FunctionMetrics(file=row[6], function=row[7], line=int(row[9]), values=values)
            )
        except ValueError as exc:
            raise ComplexityInputError(f"malformed lizard CSV row {row}: {exc}") from exc
    return functions


def run_lizard(paths: Sequence[str], settings: Settings) -> str:
    """Run lizard and return its CSV output."""
    command = [sys.executable, "-m", "lizard", "--csv", "-ENS"]
    for language in settings.languages:
        command += ["-l", language]
    for pattern in settings.exclude:
        command += ["-x", pattern]
    command += list(paths)
    completed = subprocess.run(command, capture_output=True, text=True, check=False)
    if completed.returncode != 0:
        raise ComplexityInputError(
            f"lizard failed ({completed.returncode}): {completed.stderr.strip()}"
        )
    return completed.stdout


def evaluate(
    functions: Sequence[FunctionMetrics], limits: Mapping[str, Limit]
) -> tuple[list[str], list[str]]:
    """Return (failures, warnings) for all functions."""
    failures: list[str] = []
    warnings: list[str] = []
    for item in functions:
        for metric, limit in limits.items():
            value = item.values.get(metric)
            if value is None:
                continue
            where = f"{item.file}:{item.line}: {item.function}: {metric}={value}"
            if limit.fail_above is not None and value > limit.fail_above:
                failures.append(f"{where} exceeds {limit.fail_above}")
            elif limit.warn_above is not None and value > limit.warn_above:
                warnings.append(f"{where} above {limit.warn_above}")
    return failures, warnings


def main(argv: Sequence[str] | None = None) -> int:
    """Command-line entry point."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("paths", nargs="*", help="paths to analyze (default: complexity.paths)")
    parser.add_argument(
        "--config", type=Path, default=DEFAULT_CONFIG, help="quality gate configuration"
    )
    parser.add_argument("--csv", type=Path, help="read lizard CSV output instead of running lizard")
    parser.add_argument("--summary", type=Path, help="append a Markdown summary to this file")
    args = parser.parse_args(argv)
    try:
        settings = load_settings(args.config)
        if args.csv is not None:
            try:
                text = args.csv.read_text(encoding="utf-8")
            except OSError as exc:
                raise ComplexityInputError(f"{args.csv}: cannot read: {exc}") from exc
        else:
            if args.paths:
                missing = [path for path in args.paths if not Path(path).exists()]
                if missing:
                    raise ComplexityInputError(f"path(s) not found: {', '.join(missing)}")
                paths = list(args.paths)
            else:
                paths = [path for path in settings.paths if Path(path).exists()]
                if not paths:
                    raise ComplexityInputError("none of the configured complexity.paths exists")
            text = run_lizard(paths, settings)
        functions = parse_lizard_csv(text)
    except ComplexityInputError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return EXIT_INPUT_ERROR
    failures, warnings = evaluate(functions, settings.limits)
    print(
        f"complexity gate: {len(functions)} function(s), {len(failures)} failure(s), {len(warnings)} warning(s)"
    )
    for warning in warnings:
        print(f"warning: {warning}")
    for failure in failures:
        print(f"error: {failure}")
    if args.summary:
        lines = ["## Complexity gate", "", f"{len(functions)} function(s) analysed.", ""]
        lines += [f"- FAIL `{item}`" for item in failures] + [
            f"- warn `{item}`" for item in warnings
        ]
        with args.summary.open("a", encoding="utf-8") as handle:
            handle.write("\n".join(lines) + "\n")
    return EXIT_FAIL if failures else EXIT_PASS


if __name__ == "__main__":
    sys.exit(main())
