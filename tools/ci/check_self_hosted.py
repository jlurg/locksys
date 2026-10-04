# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
# /// script
# requires-python = ">=3.13"
# dependencies = ["pyyaml==6.0.3"]
# ///
"""Verify that self-hosted runner jobs are reachable only from trusted triggers.

A job is classified as self-hosted unless its ``runs-on`` resolves to exactly one
GitHub-hosted runner label; anything that cannot be resolved statically is
treated as self-hosted (fail closed). Local reusable workflows inherit the
triggers of every caller, transitively.

Rules enforced for every self-hosted job:

* SH001: reachable only from the allowed events (default: push, schedule,
  workflow_dispatch). ``pull_request``, ``pull_request_target``, ``workflow_run``
  and ``issue_comment`` are never allowed.
* SH002: effective ``permissions`` are declared and grant no write scope.
* SH003: ``actions/checkout`` steps set ``persist-credentials: false``.

Workflow-level rules:

* SH004: a called local reusable workflow must exist.
* SH005: a remote reusable workflow must not be reachable from a disallowed
  event, because its runner selection cannot be verified here.

Exit status: 0 = no violations, 1 = violations found, 2 = input error.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from collections.abc import Iterable, Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import yaml

DEFAULT_ALLOWED_EVENTS: tuple[str, ...] = ("push", "schedule", "workflow_dispatch")
NEVER_ALLOWED_EVENTS: frozenset[str] = frozenset(
    {"pull_request", "pull_request_target", "workflow_run", "issue_comment"}
)
CALL_EVENT = "workflow_call"
# Workspace-relative and self-repository forms of a same-repository reusable workflow.
LOCAL_WORKFLOW_PREFIXES: tuple[str, ...] = ("./.github/workflows/", "$/.github/workflows/")

EXIT_OK = 0
EXIT_VIOLATIONS = 1
EXIT_INPUT_ERROR = 2

_HOSTED_LABEL = re.compile(
    r"^(?:ubuntu-(?:latest|\d{2}\.\d{2})(?:-arm)?"
    r"|windows-(?:latest|\d{4}|11-arm)"
    r"|macos-(?:latest|\d{2})(?:-(?:intel|large|xlarge))?)$",
    re.IGNORECASE,
)
_MATRIX_ONLY = re.compile(r"^\$\{\{\s*matrix\.([A-Za-z0-9_-]+)\s*\}\}$")
_CHECKOUT_ACTION = re.compile(r"^actions/checkout@", re.IGNORECASE)


class WorkflowError(Exception):
    """Raised when a workflow file cannot be interpreted."""


@dataclass(frozen=True)
class Violation:
    """A single rule violation, located by workflow file and job id."""

    rule: str
    path: str
    job: str
    message: str

    def render(self, github: bool) -> str:
        """Return the violation as a GitHub annotation or a plain diagnostic line."""
        text = f"[{self.rule}] job '{self.job}': {self.message}"
        if github:
            return f"::error file={self.path},title=self-hosted policy {self.rule}::{text}"
        return f"{self.path}: {text}"


@dataclass(frozen=True)
class Workflow:
    """A parsed workflow file."""

    key: str
    triggers: frozenset[str]
    jobs: Mapping[str, Mapping[str, Any]]
    permissions: Any


def parse_triggers(document: Mapping[Any, Any]) -> frozenset[str]:
    """Return the event names of a workflow's ``on`` section.

    PyYAML resolves the bare key ``on`` to the boolean ``True`` (YAML 1.1), so
    both spellings are accepted.
    """
    if "on" in document:
        on_section = document["on"]
    elif True in document:
        on_section = document[True]
    else:
        raise WorkflowError("missing 'on' section")
    if isinstance(on_section, str):
        return frozenset({on_section})
    if isinstance(on_section, list) and all(isinstance(item, str) for item in on_section):
        return frozenset(on_section)
    if isinstance(on_section, Mapping):
        return frozenset(str(key) for key in on_section)
    raise WorkflowError("unsupported 'on' section format")


def load_workflow(path: Path, repo_root: Path) -> Workflow:
    """Parse one workflow file."""
    try:
        document = yaml.safe_load(path.read_text(encoding="utf-8"))
    except (OSError, yaml.YAMLError) as exc:
        raise WorkflowError(f"{path}: cannot parse: {exc}") from exc
    if not isinstance(document, Mapping):
        raise WorkflowError(f"{path}: not a mapping")
    jobs = document.get("jobs")
    if not isinstance(jobs, Mapping) or not jobs:
        raise WorkflowError(f"{path}: missing 'jobs' mapping")
    for job_id, job in jobs.items():
        if not isinstance(job, Mapping):
            raise WorkflowError(f"{path}: job '{job_id}' is not a mapping")
    try:
        triggers = parse_triggers(document)
    except WorkflowError as exc:
        raise WorkflowError(f"{path}: {exc}") from exc
    return Workflow(
        key=path.resolve().relative_to(repo_root.resolve()).as_posix(),
        triggers=triggers,
        jobs={str(job_id): job for job_id, job in jobs.items()},
        permissions=document.get("permissions"),
    )


def _matrix_values(job: Mapping[str, Any], key: str) -> list[str] | None:
    strategy = job.get("strategy")
    if not isinstance(strategy, Mapping):
        return None
    matrix = strategy.get("matrix")
    if not isinstance(matrix, Mapping):
        return None
    values: list[Any] = []
    base = matrix.get(key)
    if base is not None:
        if not isinstance(base, list):
            return None
        values.extend(base)
    include = matrix.get("include", [])
    if not isinstance(include, list):
        return None
    for entry in include:
        if not isinstance(entry, Mapping):
            return None
        if key in entry:
            values.append(entry[key])
    if not values or not all(isinstance(value, str) for value in values):
        return None
    return [str(value) for value in values]


def _expand_label(label: str, job: Mapping[str, Any]) -> list[str] | None:
    if "${{" not in label:
        return [label]
    match = _MATRIX_ONLY.match(label.strip())
    if match is None:
        return None
    values = _matrix_values(job, match.group(1))
    if values is None or any("${{" in value for value in values):
        return None
    return values


def self_hosted_reason(job: Mapping[str, Any]) -> str | None:
    """Return why a job runs on a self-hosted runner, or None if GitHub-hosted."""
    runs_on = job.get("runs-on")
    if isinstance(runs_on, str):
        entries = [runs_on]
    elif isinstance(runs_on, list) and runs_on and all(isinstance(e, str) for e in runs_on):
        entries = list(runs_on)
    else:
        return "runs-on is not a plain label or label list"
    if len(entries) != 1:
        return f"runs-on lists several labels {entries}"
    alternatives = _expand_label(entries[0], job)
    if alternatives is None:
        return f"runs-on expression '{entries[0]}' cannot be resolved statically"
    for label in alternatives:
        if label.lower() == "self-hosted" or _HOSTED_LABEL.match(label) is None:
            return f"label '{label}' is not a GitHub-hosted runner label"
    return None


def _write_scopes(permissions: Any) -> list[str] | None:
    """Return the write scopes granted, or None if the value is not understood."""
    if isinstance(permissions, str):
        if permissions == "read-all":
            return []
        if permissions == "write-all":
            return ["write-all"]
        return None
    if isinstance(permissions, Mapping):
        granted = []
        for scope, level in permissions.items():
            if level not in ("read", "none", "write"):
                return None
            if level == "write":
                granted.append(str(scope))
        return granted
    return None


def _is_false(value: Any) -> bool:
    return value is False or (isinstance(value, str) and value.strip().lower() == "false")


def _job_violations(workflow: Workflow, job_id: str, job: Mapping[str, Any]) -> list[Violation]:
    violations: list[Violation] = []
    permissions = job.get("permissions", workflow.permissions)
    if permissions is None:
        violations.append(
            Violation(
                "SH002", workflow.key, job_id, "self-hosted job must declare read-only permissions"
            )
        )
    else:
        scopes = _write_scopes(permissions)
        if scopes is None:
            violations.append(
                Violation(
                    "SH002", workflow.key, job_id, f"unrecognised permissions value {permissions!r}"
                )
            )
        elif scopes:
            violations.append(
                Violation(
                    "SH002",
                    workflow.key,
                    job_id,
                    f"self-hosted job grants write scope(s): {', '.join(sorted(scopes))}",
                )
            )
    steps = job.get("steps", [])
    if isinstance(steps, list):
        for index, step in enumerate(steps):
            if not isinstance(step, Mapping):
                continue
            uses = step.get("uses")
            if isinstance(uses, str) and _CHECKOUT_ACTION.match(uses):
                with_section = step.get("with")
                persist = (
                    with_section.get("persist-credentials")
                    if isinstance(with_section, Mapping)
                    else None
                )
                if not _is_false(persist):
                    violations.append(
                        Violation(
                            "SH003",
                            workflow.key,
                            job_id,
                            f"step {index + 1}: actions/checkout must set 'persist-credentials: false'",
                        )
                    )
    return violations


def _local_callee(uses: str) -> str | None:
    for prefix in LOCAL_WORKFLOW_PREFIXES:
        if uses.startswith(prefix):
            return ".github/workflows/" + uses[len(prefix) :].split("@", 1)[0]
    return None


def reachable_events(workflows: Mapping[str, Workflow]) -> dict[str, set[str]]:
    """Return, per workflow key, every event that can start a run reaching it."""
    reach = {key: set(workflow.triggers - {CALL_EVENT}) for key, workflow in workflows.items()}
    edges: list[tuple[str, str]] = []
    for key, workflow in workflows.items():
        for job in workflow.jobs.values():
            uses = job.get("uses")
            if isinstance(uses, str):
                callee = _local_callee(uses)
                if callee is not None and callee in workflows:
                    edges.append((key, callee))
    changed = True
    while changed:
        changed = False
        for caller, callee in edges:
            before = len(reach[callee])
            reach[callee] |= reach[caller]
            changed = changed or len(reach[callee]) != before
    return reach


def analyze(workflows: Mapping[str, Workflow], allowed_events: Iterable[str]) -> list[Violation]:
    """Apply all rules and return the violations found."""
    allowed = set(allowed_events) - NEVER_ALLOWED_EVENTS
    reach = reachable_events(workflows)
    violations: list[Violation] = []
    for key in sorted(workflows):
        workflow = workflows[key]
        disallowed = sorted(reach[key] - allowed)
        for job_id, job in workflow.jobs.items():
            uses = job.get("uses")
            if isinstance(uses, str):
                callee = _local_callee(uses)
                if callee is not None:
                    if callee not in workflows:
                        violations.append(
                            Violation(
                                "SH004", key, job_id, f"called workflow '{uses}' does not exist"
                            )
                        )
                elif disallowed:
                    violations.append(
                        Violation(
                            "SH005",
                            key,
                            job_id,
                            f"remote reusable workflow '{uses}' reachable from {', '.join(disallowed)}",
                        )
                    )
                continue
            reason = self_hosted_reason(job)
            if reason is None:
                continue
            if disallowed:
                violations.append(
                    Violation(
                        "SH001",
                        key,
                        job_id,
                        f"self-hosted job ({reason}) reachable from {', '.join(disallowed)}",
                    )
                )
            violations.extend(_job_violations(workflow, job_id, job))
    return violations


def load_workflows(workflows_dir: Path) -> dict[str, Workflow]:
    """Load every ``*.yml``/``*.yaml`` file of a workflows directory."""
    if not workflows_dir.is_dir():
        raise WorkflowError(f"{workflows_dir}: not a directory")
    repo_root = workflows_dir.resolve().parent.parent
    files = sorted([*workflows_dir.glob("*.yml"), *workflows_dir.glob("*.yaml")])
    if not files:
        raise WorkflowError(f"{workflows_dir}: no workflow files")
    workflows = [load_workflow(path, repo_root) for path in files]
    return {workflow.key: workflow for workflow in workflows}


def main(argv: Sequence[str] | None = None) -> int:
    """Command-line entry point."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--workflows-dir",
        type=Path,
        default=Path(".github/workflows"),
        help="directory holding the workflow files (default: .github/workflows)",
    )
    parser.add_argument(
        "--allow-event",
        action="append",
        dest="allowed_events",
        metavar="EVENT",
        help="event allowed to reach self-hosted jobs (repeatable; default: push, schedule, workflow_dispatch)",
    )
    args = parser.parse_args(argv)
    allowed = tuple(args.allowed_events) if args.allowed_events else DEFAULT_ALLOWED_EVENTS
    forbidden = sorted(set(allowed) & NEVER_ALLOWED_EVENTS)
    if forbidden:
        print(f"error: events can never be allowed: {', '.join(forbidden)}", file=sys.stderr)
        return EXIT_INPUT_ERROR
    try:
        workflows = load_workflows(args.workflows_dir)
    except WorkflowError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return EXIT_INPUT_ERROR
    violations = analyze(workflows, allowed)
    github = os.environ.get("GITHUB_ACTIONS") == "true"
    for violation in violations:
        print(violation.render(github))
    if violations:
        print(f"{len(violations)} self-hosted policy violation(s) in {len(workflows)} workflow(s)")
        return EXIT_VIOLATIONS
    print(f"self-hosted policy: {len(workflows)} workflow(s) checked, no violations")
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
