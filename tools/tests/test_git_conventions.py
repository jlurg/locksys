# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Unit tests for tools/git/conventions.py."""

from __future__ import annotations

import ast
import subprocess
import sys
from pathlib import Path

import pytest

from tools.git import conventions as cv
from tools.git.conventions import BranchKind

SCRIPT = Path(cv.__file__)
DEPENDABOT_GROUP_TITLE = (
    "build(deps): bump the python-dependencies group across 1 directory with 12 updates (patch)"
)


@pytest.mark.parametrize(
    ("name", "kind"),
    [
        ("feature/42-win-ctrl-sm", BranchKind.WORK),
        ("fix/7-bus-off", BranchKind.WORK),
        ("perf/3-isr", BranchKind.WORK),
        ("release/1.0.0", BranchKind.RELEASE),
        ("hotfix/1.0.1", BranchKind.HOTFIX),
        ("sync/main-to-develop-v1.0.0", BranchKind.SYNC),
        ("ext/15-fork-fix", BranchKind.EXTERNAL),
        ("spike/9-canfd", BranchKind.SPIKE),
        ("dependabot/github_actions/actions/checkout-7.0.1", BranchKind.DEPENDABOT),
        ("develop", BranchKind.DEVELOP),
        ("main", BranchKind.MAIN),
    ],
)
def test_branch_kind(name: str, kind: BranchKind) -> None:
    assert cv.branch_kind(name) is kind
    assert cv.check_branch(name) == []


@pytest.mark.parametrize(
    "name",
    [
        "claude/fix-bug",
        "feature/no-issue",
        "feature/12_underscore",
        "Feature/12-upper",
        "feature/12-trailing-",
        "feature/12--double",
        "feat/12-wrong-type",
        "release/1.0",
        "release/v1.0.0",
        "hotfix/1.0.1-rc.1",
        "sync/develop-to-main",
        "sync/main-to-develop",
        "sync/main-to-release-1.1-v1.0.0",
        "develop2",
        "",
    ],
)
def test_invalid_branch_names(name: str) -> None:
    assert cv.branch_kind(name) is None
    assert len(cv.check_branch(name)) == 1


def test_branch_length_limit_except_dependabot() -> None:
    long_work = "feature/1-" + "a" * (cv.MAX_BRANCH_LENGTH - len("feature/1-") + 1)
    assert cv.branch_kind(long_work) is BranchKind.WORK
    assert "maximum" in cv.check_branch(long_work)[0]
    long_bot = "dependabot/uv/" + "x" * 80
    assert cv.check_branch(long_bot) == []


@pytest.mark.parametrize(
    ("base", "head", "method"),
    [
        ("develop", "feature/1-x", cv.SQUASH),
        ("develop", "chore/1-seed-ci", cv.SQUASH),
        ("develop", "ext/12-fork-fix", cv.SQUASH),
        ("develop", "dependabot/pip/ruff-0.16.10", cv.SQUASH),
        ("develop", "sync/main-to-develop-v1.0.0", cv.MERGE),
        ("main", "release/1.0.0", cv.MERGE),
        ("main", "hotfix/1.0.1", cv.MERGE),
        ("release/1.0.0", "fix/11-bus-off", cv.SQUASH),
        ("release/1.0.0", "docs/12-notes", cv.SQUASH),
        ("release/1.0.0", "test/13-hil", cv.SQUASH),
        ("release/1.0.0", "build/14-prepare-1-0-0", cv.SQUASH),
        ("release/1.1.0", "sync/main-to-release-1.1.0-v1.0.1", cv.MERGE),
        ("hotfix/1.0.1", "fix/16-keep-alive", cv.SQUASH),
        ("hotfix/1.0.1", "docs/18-hotfix-notes", cv.SQUASH),
        ("hotfix/1.0.1", "test/19-regression", cv.SQUASH),
        ("hotfix/1.0.1", "build/17-version-1-0-1", cv.SQUASH),
        ("develop", "spike/9-canfd", None),
        ("develop", "release/1.0.0", None),
        ("develop", "main", None),
        ("main", "develop", None),
        ("main", "feature/1-x", None),
        ("main", "sync/main-to-develop-v1.0.0", None),
        ("release/1.0.0", "feature/1-x", None),
        ("release/1.0.0", "refactor/1-x", None),
        ("hotfix/1.0.1", "feature/1-x", None),
        ("hotfix/1.0.1", "refactor/1-x", None),
        ("release/1.2.0", "sync/main-to-release-1.1.0-v1.0.1", None),
        ("develop", "sync/main-to-release-1.1.0-v1.0.1", None),
        ("release/1.1.0", "sync/main-to-develop-v1.0.1", None),
        ("feature/1-x", "fix/2-y", None),
    ],
)
def test_merge_matrix(base: str, head: str, method: str | None) -> None:
    assert cv.merge_method(base, head) == method


@pytest.mark.parametrize(
    "title",
    [
        "feat(dcu): add cooperative scheduler",
        "fix(cgw,can)!: drop stale WinCmd frames after bus-off",
        "chore(release): 1.0.0",
        "chore(release): sync main into develop after v1.0.0",
        "revert: feat(dcu): add cooperative scheduler",
        "docs: describe the branching model",
        "build(deps-dev): bump ruff to 0.16.10",
        "ci(deps): bump actions/checkout from 7.0.0 to 7.0.1",
        "test(libs)!: cover E2E counter wrap",
        "fix: " + "a" * (cv.HEADER_MAX_LENGTH - len("fix: ")),
    ],
)
def test_valid_titles(title: str) -> None:
    assert cv.check_title(title) == []


@pytest.mark.parametrize(
    ("title", "fragment"),
    [
        ("Feat: add x", "does not match"),
        ("feat:add x", "does not match"),
        ("feat (dcu): add x", "does not match"),
        ("feature(dcu): add x", "unknown type"),
        ("feat(DCU): add x", "unknown scope"),
        ("feat(foo): add x", "unknown scope"),
        ("feat(dcu, can): add x", "unknown scope"),
        ("feat(): add x", "unknown scope"),
        ("feat: Add x", "lower-case"),
        ("feat:  add x", "lower-case"),
        ("feat: add x.", "period"),
        ("feat: add x ", "whitespace"),
        ("feat: ab", "at least"),
        ("feat: " + "a" * (cv.SUBJECT_MAX_LENGTH + 1), "subject is"),
        ("fix: " + "a" * (cv.HEADER_MAX_LENGTH - len("fix: ") + 1), "header is"),
        ('Revert "feat(dcu): add x"', "revert:"),
    ],
)
def test_invalid_titles(title: str, fragment: str) -> None:
    errors = cv.check_title(title)
    assert errors
    assert any(fragment in error for error in errors), errors


def test_title_length_limit_can_be_lifted() -> None:
    title = DEPENDABOT_GROUP_TITLE
    assert cv.check_title(title) != []
    assert cv.check_title(title, limit_length=False) == []


@pytest.mark.parametrize(
    "message",
    [
        "feat(dcu): add scheduler\n",
        "feat(dcu): add scheduler\n\nBody line.\n\nCloses: #3\n",
        "# Please enter the commit message\nfix(app): handle resume\n# comment\n",
        "fix(app): handle resume\n# ------------------------ >8 ------------------------\n+diff\n",
        "Merge branch 'develop' into feature/1-x\n",
        "Merge remote-tracking branch 'origin/develop' into sync/main-to-develop-v1.0.0\n",
        "fixup! feat(dcu): add scheduler\n",
        "squash! feat(dcu): add scheduler\n",
        "amend! feat(dcu): add scheduler\n",
        "",
        "# only comments\n",
    ],
)
def test_valid_commit_messages(message: str) -> None:
    assert cv.check_commit_message(message) == []


@pytest.mark.parametrize(
    ("message", "fragment"),
    [
        ("Add scheduler\n", "does not match"),
        ("feat(dcu): add scheduler\nBody without blank line\n", "blank line"),
        ('Revert "feat(dcu): add scheduler"\n\nThis reverts commit abc.\n', "revert:"),
        ("WIP\n", "does not match"),
    ],
)
def test_invalid_commit_messages(message: str, fragment: str) -> None:
    errors = cv.check_commit_message(message)
    assert any(fragment in error for error in errors), errors


@pytest.mark.parametrize(
    ("body", "expected"),
    [
        ("Closes #12", True),
        ("## Links\nCloses: #12 · Requirements: SWR-DCU-042", True),
        ("Refs: #3", True),
        ("fixes jlurg/locksys#7", True),
        ("Resolved #9", True),
        ("Closes #  · Requirements: SWR-...", False),
        ("see #12", False),
        ("", False),
    ],
)
def test_issue_reference(body: str, expected: bool) -> None:
    assert cv.has_issue_reference(body) is expected


@pytest.mark.parametrize(
    ("base", "head", "author", "exempt"),
    [
        ("develop", "dependabot/uv/ruff-0.16.10", "dependabot[bot]", True),
        ("develop", "sync/main-to-develop-v1.0.0", "jlurg", True),
        ("main", "release/1.0.0", "jlurg", True),
        ("main", "hotfix/1.0.1", None, True),
        ("develop", "feature/1-x", "jlurg", False),
        ("release/1.0.0", "fix/2-y", "jlurg", False),
    ],
)
def test_issue_link_exemptions(base: str, head: str, author: str | None, exempt: bool) -> None:
    assert cv.is_issue_link_exempt(base, head, author) is exempt


def test_pull_request_valid() -> None:
    errors = cv.check_pull_request(
        "develop",
        "feature/42-win-ctrl-sm",
        "feat(dcu): add window state machine",
        "jlurg",
        "Closes #42",
    )
    assert errors == []


def test_pull_request_collects_all_errors() -> None:
    errors = cv.check_pull_request("main", "feature/42-x", "Add stuff.", "jlurg", "no reference")
    assert any("not allowed" in error for error in errors)
    assert any("does not match" in error for error in errors)
    assert any("reference an issue" in error for error in errors)


def test_pull_request_from_invalid_branch() -> None:
    errors = cv.check_pull_request("develop", "claude/x", "feat(dcu): add x")
    assert len(errors) == 1
    assert "naming rules" in errors[0]


def test_pull_request_spike_is_never_merged() -> None:
    errors = cv.check_pull_request(
        "develop", "spike/9-canfd", "chore: evaluate CAN FD", body="Refs #9"
    )
    assert errors == [
        "merging 'spike/9-canfd' into 'develop' is not allowed by the branching model"
    ]


def test_pull_request_dependabot_title_without_length_limit() -> None:
    title = DEPENDABOT_GROUP_TITLE
    errors = cv.check_pull_request(
        "develop", "dependabot/uv/python-abc", title, "dependabot[bot]", "Bumps the group."
    )
    assert errors == []


def test_pull_request_body_check_is_optional() -> None:
    assert cv.check_pull_request("develop", "fix/3-x", "fix(app): handle resume") == []


def test_cli_branch_prints_kind(capsys: pytest.CaptureFixture[str]) -> None:
    assert cv.main(["branch", "release/1.2.0"]) == cv.EXIT_OK
    assert capsys.readouterr().out.strip() == "release"
    assert cv.main(["branch", "claude/x"]) == cv.EXIT_VIOLATION


def test_cli_title_and_head(capsys: pytest.CaptureFixture[str]) -> None:
    assert cv.main(["title", "feat(app): add pairing screen"]) == cv.EXIT_OK
    long_title = "build(deps): " + "bump " * 20 + "x"
    assert cv.main(["title", long_title]) == cv.EXIT_VIOLATION
    assert cv.main(["title", long_title, "--head", "dependabot/uv/x"]) == cv.EXIT_OK
    assert "maximum" in capsys.readouterr().err


def test_cli_github_format_escapes(capsys: pytest.CaptureFixture[str]) -> None:
    assert cv.main(["--format", "github", "title", "100% wrong"]) == cv.EXIT_VIOLATION
    out = capsys.readouterr().out
    assert out.startswith("::error title=pr-policy::")
    assert "100%25 wrong" in out


def test_cli_merge_method(capsys: pytest.CaptureFixture[str]) -> None:
    assert cv.main(["merge-method", "--base", "main", "--head", "release/1.0.0"]) == cv.EXIT_OK
    assert capsys.readouterr().out.strip() == cv.MERGE
    assert cv.main(["merge-method", "--base", "main", "--head", "fix/1-x"]) == cv.EXIT_VIOLATION


def test_cli_commit_msg(tmp_path: Path) -> None:
    msg = tmp_path / "COMMIT_EDITMSG"
    msg.write_text("fix(hil): stabilise UART parser\n", encoding="utf-8")
    assert cv.main(["commit-msg", str(msg)]) == cv.EXIT_OK
    msg.write_text("Fixed stuff\n", encoding="utf-8")
    assert cv.main(["commit-msg", str(msg)]) == cv.EXIT_VIOLATION
    assert cv.main(["commit-msg", str(tmp_path / "missing")]) == cv.EXIT_USAGE


def test_cli_pr_with_body_file(tmp_path: Path) -> None:
    body = tmp_path / "body.txt"
    body.write_text("## Summary\nAdds X.\n## Links\nCloses #5\n", encoding="utf-8")
    base_args = ["pr", "--base", "develop", "--head", "feature/5-x", "--author", "jlurg"]
    assert cv.main([*base_args, "--title", "feat(tools): add x", "--body-file", str(body)]) == 0
    body.write_text("## Summary\nAdds X.\n", encoding="utf-8")
    assert cv.main([*base_args, "--title", "feat(tools): add x", "--body-file", str(body)]) == 1


def test_script_entry_point() -> None:
    result = subprocess.run(
        [sys.executable, str(SCRIPT), "merge-method", "--base", "develop", "--head", "fix/1-x"],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0
    assert result.stdout.strip() == cv.SQUASH


def test_module_uses_only_the_standard_library() -> None:
    tree = ast.parse(SCRIPT.read_text(encoding="utf-8"))
    modules: set[str] = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            modules.update(alias.name.split(".")[0] for alias in node.names)
        elif isinstance(node, ast.ImportFrom) and node.module and node.level == 0:
            modules.add(node.module.split(".")[0])
    assert modules <= set(sys.stdlib_module_names) | {"__future__"}
