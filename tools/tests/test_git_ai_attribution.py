# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Unit tests for tools/git/ai_attribution.py."""

from __future__ import annotations

import ast
import io
import subprocess
import sys
from pathlib import Path

import pytest

from tools.git import ai_attribution as aa

SCRIPT = Path(aa.__file__)
ROBOT = "\U0001f916"
DEFAULT_COMMIT_TRAILER = "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
DEFAULT_PR_LINE = f"{ROBOT} Generated with [Claude Code](https://claude.com/claude-code)"
PR_TEMPLATE_HYGIENE = (
    "- [ ] No AI attribution (no AI Co-Authored-By/Claude-Session trailers, "
    'no "Generated with ..." lines)'
)


def rules_of(findings: list[aa.Finding]) -> list[str]:
    return [finding.rule for finding in findings]


@pytest.mark.parametrize(
    ("text", "rule"),
    [
        (f"feat(dcu): add scheduler\n\n{DEFAULT_COMMIT_TRAILER}\n", "ai-trailer"),
        ("fix: x\n\nco-authored-by: claude <noreply@anthropic.com>", "ai-trailer"),
        ("fix: x\n\nCo-authored-by: Helper <helper@anthropic.com>", "ai-trailer"),
        ("fix: x\n\nSigned-off-by: Claude <noreply@anthropic.com>", "ai-trailer"),
        ("fix: x\n\n  Assisted-by: Claude Code", "ai-trailer"),
        ("fix: x\n\nClaude-Session: https://claude.ai/code/session_01", "claude-session-trailer"),
        (f"Summary\n\n{DEFAULT_PR_LINE}\n", "robot-generated-line"),
        ("Generated with Claude Code", "generated-with-claude"),
        ("Body text\nCreated using Claude.\n", "generated-with-claude"),
        ("See https://claude.ai/code/session_01ABC for details", "claude-code-link"),
        ("Docs: https://www.claude.com/claude-code", "claude-code-link"),
    ],
)
def test_forbidden_text_is_detected(text: str, rule: str) -> None:
    findings = aa.scan_text(text, "msg")
    assert rules_of(findings) == [rule]


@pytest.mark.parametrize(
    "text",
    [
        "feat(dcu): add scheduler\n\nCloses: #12\nReq: SWR-DCU-042\n",
        "fix(cgw): handle bus-off\n\nCo-authored-by: Jane Doe <jane@example.com>\n",
        PR_TEMPLATE_HYGIENE,
        "docs(repo): explain why claude is not listed as an author",
        "chore: mention Claudette's review",
        "Reviewed-by: jlurg",
    ],
)
def test_clean_text_passes(text: str) -> None:
    assert aa.scan_text(text, "msg") == []


def test_one_finding_per_line_with_position_and_excerpt() -> None:
    text = f"title\n\n{DEFAULT_PR_LINE}\n{DEFAULT_COMMIT_TRAILER}\n"
    findings = aa.scan_text(text, "body")
    assert [(f.line, f.rule) for f in findings] == [
        (3, "robot-generated-line"),
        (4, "ai-trailer"),
    ]
    assert str(findings[1]) == f"body:4: ai-trailer: {DEFAULT_COMMIT_TRAILER}"


def test_long_excerpt_is_truncated() -> None:
    line = "Co-Authored-By: Claude " + "x" * 300
    (finding,) = aa.scan_text(line, "msg")
    assert len(finding.excerpt) == aa.EXCERPT_MAX
    assert finding.excerpt.endswith("...")


def test_git_comments_and_verbose_diff_are_ignored() -> None:
    message = (
        "feat(tools): add attribution checker\n"
        "\n"
        "# Co-Authored-By: Claude <noreply@anthropic.com>\n"
        "# ------------------------ >8 ------------------------\n"
        "# Do not modify or remove the line above.\n"
        "+Co-Authored-By: Claude <noreply@anthropic.com>\n"
    )
    stripped = aa.strip_git_comments(message)
    assert stripped == "feat(tools): add attribution checker\n"
    assert aa.scan_text(stripped, "COMMIT_EDITMSG") == []


@pytest.mark.parametrize(
    "command",
    [
        "git commit -m \"$(cat <<'EOF'\nfeat: x\n\n" + DEFAULT_COMMIT_TRAILER + '\nEOF\n)"',
        f'git commit -m "feat: x" -m "{DEFAULT_COMMIT_TRAILER}"',
        "git commit -m $'feat: x\\n\\nCo-Authored-By: Claude <noreply@anthropic.com>'",
        'git commit --trailer "Co-authored-by=Claude <noreply@anthropic.com>" -m "feat: x"',
        'git commit --trailer "Claude-Session: abc" -m "feat: x"',
        'git commit --author="Claude <noreply@anthropic.com>" -m "feat: x"',
        "git -c user.name=Claude -c user.email=x@example.com commit -m 'feat: x'",
        "GIT_COMMITTER_EMAIL=bot@anthropic.com git commit -m 'feat: x'",
        f'gh pr create --title "feat: x" --body "Closes #1\n\n{DEFAULT_PR_LINE}"',
    ],
)
def test_forbidden_commands_are_detected(command: str) -> None:
    assert aa.scan_command(command) != []


@pytest.mark.parametrize(
    "command",
    [
        'git commit -m "feat(dcu): add scheduler" -m "Closes: #12"',
        'git commit --author="jlurg <109570806+jlurg@users.noreply.github.com>" -m "fix: x"',
        'gh pr create --base develop --title "feat(app): pairing" --body "Closes #3"',
        "git -c core.autocrlf=false commit -F msg.txt",
    ],
)
def test_clean_commands_pass(command: str) -> None:
    assert aa.scan_command(command) == []


@pytest.mark.parametrize(
    "identity",
    ["Claude", "claude[bot]", "Claude Opus 5.5", "noreply@anthropic.com", "x+claude@example.com"],
)
def test_ai_identities_are_detected(identity: str) -> None:
    assert len(aa.scan_identities([identity], "ids")) == 1


@pytest.mark.parametrize(
    "identity",
    [
        "jlurg",
        "109570806+jlurg@users.noreply.github.com",
        "GitHub",
        "noreply@github.com",
        "dependabot[bot]",
        "Claudette Smith",
        "",
    ],
)
def test_human_identities_pass(identity: str) -> None:
    assert aa.scan_identities([identity], "ids") == []


def test_identity_findings_report_line_numbers() -> None:
    findings = aa.scan_identities(["jlurg", "", "Claude"], "identities.txt")
    assert [(f.line, f.rule) for f in findings] == [(3, "ai-identity-name")]


def test_main_commit_msg_violation(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    msg = tmp_path / "COMMIT_EDITMSG"
    msg.write_text(f"feat: x\n\n{DEFAULT_COMMIT_TRAILER}\n", encoding="utf-8")
    assert aa.main(["--commit-msg", str(msg)]) == aa.EXIT_FOUND
    err = capsys.readouterr().err
    assert "AI attribution is forbidden" in err
    assert "COMMIT_EDITMSG:3: ai-trailer" in err


def test_main_commit_msg_clean(tmp_path: Path) -> None:
    msg = tmp_path / "COMMIT_EDITMSG"
    msg.write_text("feat(dcu): add scheduler\n\nCloses: #3\n", encoding="utf-8")
    assert aa.main(["--commit-msg", str(msg)]) == aa.EXIT_CLEAN


def test_main_stdin(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(
        sys, "stdin", io.StringIO(f'git commit -m "x" -m "{DEFAULT_COMMIT_TRAILER}"')
    )
    assert aa.main(["--stdin"]) == aa.EXIT_FOUND
    monkeypatch.setattr(sys, "stdin", io.StringIO('git commit -m "feat: x"'))
    assert aa.main(["--stdin"]) == aa.EXIT_CLEAN


def test_main_messages_and_identities(tmp_path: Path) -> None:
    messages = tmp_path / "messages.txt"
    identities = tmp_path / "identities.txt"
    messages.write_text("feat: x\n\nCloses #1\n", encoding="utf-8")
    identities.write_text("jlurg\n109570806+jlurg@users.noreply.github.com\n", encoding="utf-8")
    args = ["--messages", str(messages), "--identities", str(identities)]
    assert aa.main(args) == aa.EXIT_CLEAN
    identities.write_text("jlurg\nClaude\n", encoding="utf-8")
    assert aa.main(args) == aa.EXIT_FOUND
    assert aa.main(["--identities", str(identities)]) == aa.EXIT_FOUND


@pytest.mark.parametrize(
    "argv",
    [[], ["--stdin", "--commit-msg", "x"], ["--stdin", "--messages", "x"]],
)
def test_main_rejects_ambiguous_modes(argv: list[str]) -> None:
    assert aa.main(argv) == aa.EXIT_USAGE


def test_main_reports_missing_file(tmp_path: Path) -> None:
    assert aa.main(["--commit-msg", str(tmp_path / "missing")]) == aa.EXIT_USAGE


def test_script_entry_point() -> None:
    result = subprocess.run(
        [sys.executable, str(SCRIPT), "--stdin"],
        input=f"git commit -m 'x' --trailer '{DEFAULT_COMMIT_TRAILER}'",
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == aa.EXIT_FOUND
    assert "inline-ai-trailer" in result.stderr or "ai-trailer" in result.stderr


def test_module_uses_only_the_standard_library() -> None:
    tree = ast.parse(SCRIPT.read_text(encoding="utf-8"))
    modules: set[str] = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            modules.update(alias.name.split(".")[0] for alias in node.names)
        elif isinstance(node, ast.ImportFrom) and node.module and node.level == 0:
            modules.add(node.module.split(".")[0])
    assert modules <= set(sys.stdlib_module_names) | {"__future__"}
