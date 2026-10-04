# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Unit tests for tools/ci/check_self_hosted.py."""

from __future__ import annotations

import textwrap
from pathlib import Path

import pytest

from tools.ci import check_self_hosted as sh

REPO_ROOT = Path(__file__).resolve().parents[3]

SELF_HOSTED_JOB = """
  build:
    runs-on: [self-hosted, windows, iar]
    permissions:
      contents: read
    steps:
      - uses: actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1 # v7.0.1
        with:
          persist-credentials: false
      - run: echo build
"""


def _write(root: Path, name: str, text: str) -> Path:
    workflows = root / ".github" / "workflows"
    workflows.mkdir(parents=True, exist_ok=True)
    path = workflows / name
    path.write_text(textwrap.dedent(text).lstrip(), encoding="utf-8")
    return workflows


def _rules(workflows: Path) -> list[str]:
    return [
        violation.rule
        for violation in sh.analyze(sh.load_workflows(workflows), sh.DEFAULT_ALLOWED_EVENTS)
    ]


def test_push_only_self_hosted_job_passes(tmp_path: Path) -> None:
    workflows = _write(
        tmp_path, "iar.yml", "on:\n  push:\n  workflow_dispatch:\njobs:" + SELF_HOSTED_JOB
    )
    assert _rules(workflows) == []


@pytest.mark.parametrize(
    "event",
    [
        "pull_request",
        "pull_request_target",
        "workflow_run",
        "issue_comment",
        "release",
        "merge_group",
    ],
)
def test_untrusted_event_reaching_self_hosted_job_fails(tmp_path: Path, event: str) -> None:
    workflows = _write(tmp_path, "iar.yml", f"on:\n  push:\n  {event}:\njobs:" + SELF_HOSTED_JOB)
    violations = sh.analyze(sh.load_workflows(workflows), sh.DEFAULT_ALLOWED_EVENTS)
    assert [v.rule for v in violations] == ["SH001"]
    assert event in violations[0].message


@pytest.mark.parametrize("on_section", ["on: pull_request", "on: [push, pull_request]"])
def test_string_and_list_trigger_forms(tmp_path: Path, on_section: str) -> None:
    workflows = _write(tmp_path, "iar.yml", on_section + "\njobs:" + SELF_HOSTED_JOB)
    assert _rules(workflows) == ["SH001"]


def test_reusable_workflow_inherits_caller_triggers(tmp_path: Path) -> None:
    _write(tmp_path, "_build.yml", "on:\n  workflow_call:\njobs:" + SELF_HOSTED_JOB)
    _write(
        tmp_path,
        "ci.yml",
        """
        on: pull_request
        jobs:
          call:
            uses: ./.github/workflows/_build.yml
        """,
    )
    workflows = _write(
        tmp_path,
        "push.yml",
        """
        on: push
        jobs:
          call:
            uses: ./.github/workflows/_build.yml
        """,
    )
    violations = sh.analyze(sh.load_workflows(workflows), sh.DEFAULT_ALLOWED_EVENTS)
    assert [(v.rule, v.path) for v in violations] == [("SH001", ".github/workflows/_build.yml")]
    assert "pull_request" in violations[0].message


def test_self_repository_reference_is_resolved(tmp_path: Path) -> None:
    _write(tmp_path, "_build.yml", "on:\n  workflow_call:\njobs:" + SELF_HOSTED_JOB)
    workflows = _write(
        tmp_path,
        "ci.yml",
        "on: pull_request\njobs:\n  call:\n    uses: $/.github/workflows/_build.yml\n",
    )
    assert _rules(workflows) == ["SH001"]


def test_reachability_is_transitive(tmp_path: Path) -> None:
    _write(tmp_path, "c.yml", "on:\n  workflow_call:\njobs:" + SELF_HOSTED_JOB)
    _write(
        tmp_path,
        "b.yml",
        "on: workflow_call\njobs:\n  call:\n    uses: ./.github/workflows/c.yml\n",
    )
    workflows = _write(
        tmp_path,
        "a.yml",
        "on: issue_comment\njobs:\n  call:\n    uses: ./.github/workflows/b.yml\n",
    )
    assert _rules(workflows) == ["SH001"]


def test_uncalled_reusable_workflow_is_not_reachable(tmp_path: Path) -> None:
    workflows = _write(tmp_path, "_build.yml", "on: workflow_call\njobs:" + SELF_HOSTED_JOB)
    assert _rules(workflows) == []


def test_custom_label_without_self_hosted_is_treated_as_self_hosted(tmp_path: Path) -> None:
    workflows = _write(
        tmp_path,
        "ci.yml",
        """
        on: pull_request
        jobs:
          build:
            runs-on: iar
            permissions: {contents: read}
            steps:
              - run: echo
        """,
    )
    assert _rules(workflows) == ["SH001"]


def test_hosted_matrix_labels_pass_and_unresolved_expression_fails_closed(tmp_path: Path) -> None:
    workflows = _write(
        tmp_path,
        "ci.yml",
        """
        on: pull_request
        jobs:
          hosted:
            strategy:
              matrix:
                os: [ubuntu-24.04, windows-2025]
                include:
                  - os: macos-26
            runs-on: ${{ matrix.os }}
            steps:
              - run: echo
          dynamic:
            runs-on: ${{ fromJSON(vars.RUNNER) }}
            permissions: {contents: read}
            steps:
              - run: echo
        """,
    )
    violations = sh.analyze(sh.load_workflows(workflows), sh.DEFAULT_ALLOWED_EVENTS)
    assert [(v.rule, v.job) for v in violations] == [("SH001", "dynamic")]


def test_runner_group_is_treated_as_self_hosted() -> None:
    assert sh.self_hosted_reason({"runs-on": {"group": "lab", "labels": ["iar"]}}) is not None
    assert sh.self_hosted_reason({"runs-on": "ubuntu-24.04"}) is None
    assert sh.self_hosted_reason({"runs-on": ["ubuntu-24.04"]}) is None
    assert sh.self_hosted_reason({"runs-on": ["ubuntu-24.04", "self-hosted"]}) is not None


def test_remote_reusable_workflow_from_pull_request_fails(tmp_path: Path) -> None:
    workflows = _write(
        tmp_path,
        "ci.yml",
        """
        on: pull_request
        jobs:
          call:
            uses: example/repo/.github/workflows/build.yml@0123456789abcdef0123456789abcdef01234567
        """,
    )
    assert _rules(workflows) == ["SH005"]


def test_missing_local_callee_is_reported(tmp_path: Path) -> None:
    workflows = _write(
        tmp_path, "ci.yml", "on: push\njobs:\n  call:\n    uses: ./.github/workflows/none.yml\n"
    )
    assert _rules(workflows) == ["SH004"]


@pytest.mark.parametrize(
    ("permissions", "expected"),
    [
        ("    permissions:\n      contents: write\n", ["SH002"]),
        ("    permissions: write-all\n", ["SH002"]),
        ("    permissions: read-all\n", []),
        ("    permissions: {}\n", []),
        ("", ["SH002"]),
    ],
)
def test_self_hosted_job_permissions(tmp_path: Path, permissions: str, expected: list[str]) -> None:
    text = (
        "on: push\n"
        "jobs:\n"
        "  build:\n"
        "    runs-on: [self-hosted, iar]\n"
        f"{permissions}"
        "    steps:\n"
        "      - run: echo\n"
    )
    workflows = _write(tmp_path, "iar.yml", text)
    assert _rules(workflows) == expected


def test_workflow_level_permissions_apply_to_self_hosted_job(tmp_path: Path) -> None:
    workflows = _write(
        tmp_path,
        "iar.yml",
        """
        on: push
        permissions:
          contents: read
        jobs:
          build:
            runs-on: [self-hosted, iar]
            steps:
              - run: echo
        """,
    )
    assert _rules(workflows) == []


@pytest.mark.parametrize(
    "with_section",
    [
        "",
        "        with:\n          persist-credentials: true\n",
        "        with:\n          clean: true\n",
    ],
)
def test_checkout_must_not_persist_credentials(tmp_path: Path, with_section: str) -> None:
    text = (
        "on: push\n"
        "jobs:\n"
        "  build:\n"
        "    runs-on: [self-hosted, iar]\n"
        "    permissions: {contents: read}\n"
        "    steps:\n"
        "      - uses: actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1 # v7.0.1\n"
        f"{with_section}"
    )
    workflows = _write(tmp_path, "iar.yml", text)
    assert _rules(workflows) == ["SH003"]


def test_hosted_job_rules_do_not_apply_to_github_hosted_jobs(tmp_path: Path) -> None:
    workflows = _write(
        tmp_path,
        "ci.yml",
        """
        on: pull_request_target
        jobs:
          lint:
            runs-on: ubuntu-24.04
            steps:
              - uses: actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1 # v7.0.1
        """,
    )
    assert _rules(workflows) == []


def test_invalid_workflow_is_an_input_error(tmp_path: Path) -> None:
    workflows = _write(tmp_path, "bad.yml", "on: push\n")
    assert sh.main(["--workflows-dir", str(workflows)]) == sh.EXIT_INPUT_ERROR


def test_never_allowed_event_cannot_be_allowed(tmp_path: Path) -> None:
    workflows = _write(tmp_path, "iar.yml", "on: push\njobs:" + SELF_HOSTED_JOB)
    assert (
        sh.main(["--workflows-dir", str(workflows), "--allow-event", "pull_request"])
        == sh.EXIT_INPUT_ERROR
    )


def test_cli_reports_violations(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    workflows = _write(tmp_path, "iar.yml", "on: pull_request\njobs:" + SELF_HOSTED_JOB)
    assert sh.main(["--workflows-dir", str(workflows)]) == sh.EXIT_VIOLATIONS
    assert "SH001" in capsys.readouterr().out


def test_github_annotation_format() -> None:
    violation = sh.Violation("SH001", ".github/workflows/x.yml", "build", "message")
    assert violation.render(github=True).startswith("::error file=.github/workflows/x.yml,")
    assert violation.render(github=False) == ".github/workflows/x.yml: [SH001] job 'build': message"


def test_repository_workflows_comply() -> None:
    workflows = REPO_ROOT / ".github" / "workflows"
    if not workflows.is_dir():
        pytest.skip("repository workflows not present")
    assert sh.analyze(sh.load_workflows(workflows), sh.DEFAULT_ALLOWED_EVENTS) == []
