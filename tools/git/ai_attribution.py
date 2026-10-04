#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Detect AI-tool attribution in commit messages, pull-request text, identities and commands.

LockSys forbids AI attribution in its history (see ``CLAUDE.md``). This module holds the single
set of detection rules used by every enforcement layer:

* the ``no-ai-attribution`` commit-msg hook in ``.pre-commit-config.yaml``;
* the Claude Code PreToolUse hook ``.claude/hooks/block-ai-attribution.sh``;
* the required ``pr-policy`` CI check (commit messages, PR title/body, author and committer).

Command-line interface::

    ai_attribution.py --commit-msg FILE
        Scan a commit message file. Git comment lines and everything below the
        scissors line written by ``git commit --verbose`` are ignored.

    ai_attribution.py --stdin
        Scan standard input as free text, typically a shell command line. Command-line
        forms (inline trailers, ``--trailer``, ``--author`` and identity overrides) are
        detected in addition to the message rules.

    ai_attribution.py [--messages FILE] [--identities FILE]
        Scan message text (commit messages, PR title and body) and an identity list
        (author/committer names and e-mail addresses, one per line).

Exit status: 0 when nothing is found, 1 when attribution is found, 2 on usage or I/O errors.

The module depends on the standard library only and supports Python 3.9 or later, so it runs
from pre-commit, the Lab Host and CI without a project environment.
"""

from __future__ import annotations

import argparse
import re
import sys
from collections.abc import Iterable, Sequence
from dataclasses import dataclass
from pathlib import Path

ROBOT_FACE = "\U0001f916"
SCISSORS_MARK = "------------------------ >8 ------------------------"
EXCERPT_MAX = 120

EXIT_CLEAN = 0
EXIT_FOUND = 1
EXIT_USAGE = 2


@dataclass(frozen=True)
class Rule:
    """A named detection pattern."""

    name: str
    pattern: re.Pattern[str]


@dataclass(frozen=True)
class Finding:
    """A rule match located in a named input."""

    source: str
    line: int
    rule: str
    excerpt: str

    def __str__(self) -> str:
        return f"{self.source}:{self.line}: {self.rule}: {self.excerpt}"


_AI_NAME = r"(?:\bclaude\b|anthropic)"

TEXT_RULES: tuple[Rule, ...] = (
    Rule("ai-trailer", re.compile(r"(?im)^[ \t]*[a-z]+(?:-[a-z]+)*-by[ \t]*:.*" + _AI_NAME)),
    Rule("claude-session-trailer", re.compile(r"(?im)^[ \t]*claude-session[ \t]*:")),
    Rule(
        "generated-with-claude",
        re.compile(
            r"(?i)\b(?:generated|created|written|authored|built)[ \t]+(?:with|by|using)[ \t]+"
            r"\[?claude\b"
        ),
    ),
    Rule(
        "claude-code-link",
        re.compile(r"(?i)https?://(?:www\.)?claude\.(?:com|ai)/(?:claude-code|code)\b"),
    ),
    Rule("robot-generated-line", re.compile(ROBOT_FACE + r"[ \t]*generated\b", re.IGNORECASE)),
)

COMMAND_RULES: tuple[Rule, ...] = (
    Rule(
        "inline-ai-trailer", re.compile(r"(?i)\b[a-z]+(?:-[a-z]+)*-by[ \t]*[:=][^\n]*?" + _AI_NAME)
    ),
    Rule("inline-claude-session", re.compile(r"(?i)\bclaude-session[ \t]*[:=]")),
    Rule(
        "ai-identity-override",
        re.compile(
            r"(?i)(?:--author|\buser\.(?:name|email)|\bGIT_(?:AUTHOR|COMMITTER)_(?:NAME|EMAIL))"
            r"[ \t]*=?[ \t]*[\"']?[^\"'\n]*?" + _AI_NAME
        ),
    ),
)

IDENTITY_RULES: tuple[Rule, ...] = (
    Rule("ai-identity-name", re.compile(r"(?i)\bclaude\b")),
    Rule("ai-identity-email", re.compile(r"(?i)@anthropic\.com$")),
)


def _excerpt(text: str) -> str:
    text = text.strip()
    if len(text) > EXCERPT_MAX:
        return text[: EXCERPT_MAX - 3] + "..."
    return text


def strip_git_comments(message: str, comment_char: str = "#") -> str:
    """Return a commit message without Git comment lines and verbose diff.

    @param message Raw content of the commit message file.
    @param comment_char Git ``core.commentChar`` (``#`` by default).
    @return The message as Git stores it with the default ``strip`` clean-up mode.
    """
    kept: list[str] = []
    for line in message.splitlines():
        if line.startswith(comment_char):
            if SCISSORS_MARK in line:
                break
            continue
        kept.append(line)
    return "\n".join(kept)


def scan_text(text: str, source: str, rules: Sequence[Rule] = TEXT_RULES) -> list[Finding]:
    """Scan free text and report the first rule match of every offending line.

    @param text Text to scan (may span several lines).
    @param source Name of the input, used in the findings.
    @param rules Rules to apply.
    @return Findings ordered by line, at most one per line.
    """
    lines = text.split("\n")
    first_match: dict[int, tuple[int, str]] = {}
    for rule in rules:
        for match in rule.pattern.finditer(text):
            line_no = text.count("\n", 0, match.start()) + 1
            known = first_match.get(line_no)
            if known is None or match.start() < known[0]:
                first_match[line_no] = (match.start(), rule.name)
    return [
        Finding(source, line_no, rule_name, _excerpt(lines[line_no - 1]))
        for line_no, (_, rule_name) in sorted(first_match.items())
    ]


def scan_command(command: str, source: str = "<stdin>") -> list[Finding]:
    """Scan a shell command line with the message rules and the command-line rules.

    @param command Command text as passed to the shell.
    @param source Name of the input, used in the findings.
    @return Findings ordered by line, at most one per line.
    """
    return scan_text(command, source, TEXT_RULES + COMMAND_RULES)


def scan_identities(identities: Iterable[str], source: str) -> list[Finding]:
    """Scan author and committer names or e-mail addresses, one identity per item.

    @param identities Names and e-mail addresses.
    @param source Name of the input, used in the findings.
    @return One finding per matching identity.
    """
    findings: list[Finding] = []
    for line_no, raw in enumerate(identities, start=1):
        identity = raw.strip()
        if not identity:
            continue
        for rule in IDENTITY_RULES:
            if rule.pattern.search(identity):
                findings.append(Finding(source, line_no, rule.name, _excerpt(identity)))
                break
    return findings


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="ai_attribution.py",
        description="Fail when AI-tool attribution is present (see CLAUDE.md).",
    )
    parser.add_argument("--commit-msg", metavar="FILE", type=Path, help="commit message file")
    parser.add_argument("--stdin", action="store_true", help="scan standard input (command text)")
    parser.add_argument("--messages", metavar="FILE", type=Path, help="message text to scan")
    parser.add_argument(
        "--identities", metavar="FILE", type=Path, help="names and e-mail addresses, one per line"
    )
    return parser


def run(args: argparse.Namespace) -> list[Finding]:
    """Execute the scan selected by the parsed command-line arguments.

    @param args Parsed arguments from the command-line parser.
    @return All findings.
    @pre Exactly one mode is selected.
    """
    if args.commit_msg is not None:
        message = strip_git_comments(_read(args.commit_msg))
        return scan_text(message, args.commit_msg.name)
    if args.stdin:
        return scan_command(sys.stdin.read())
    findings: list[Finding] = []
    if args.messages is not None:
        findings += scan_text(_read(args.messages), args.messages.name)
    if args.identities is not None:
        findings += scan_identities(_read(args.identities).splitlines(), args.identities.name)
    return findings


def main(argv: Sequence[str] | None = None) -> int:
    """Command-line entry point.

    @param argv Arguments without the program name; ``None`` uses ``sys.argv``.
    @return Process exit status.
    """
    parser = _build_parser()
    args = parser.parse_args(argv)
    selected = [
        args.commit_msg is not None,
        bool(args.stdin),
        args.messages is not None or args.identities is not None,
    ]
    if selected.count(True) != 1:
        parser.print_usage(sys.stderr)
        print(
            "error: select exactly one of --commit-msg, --stdin or --messages/--identities",
            file=sys.stderr,
        )
        return EXIT_USAGE
    try:
        findings = run(args)
    except OSError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return EXIT_USAGE
    if not findings:
        return EXIT_CLEAN
    print("error: AI attribution is forbidden in this repository (see CLAUDE.md):", file=sys.stderr)
    for finding in findings:
        print(f"  {finding}", file=sys.stderr)
    print(
        "Remove AI co-author or session trailers, 'Generated with' lines and AI identities.",
        file=sys.stderr,
    )
    return EXIT_FOUND


if __name__ == "__main__":
    sys.exit(main())
