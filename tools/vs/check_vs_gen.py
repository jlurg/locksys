# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Check the generated Visual State engines for forbidden constructs.

Rules for ``gen_vs/release/`` (the variant linked into the Hil and Release images):

* VSG001 no heap: ``malloc``, ``calloc``, ``realloc``, ``free``;
* VSG002 no ``va_arg`` (the deduct functions may be declared variadic, permit DP-05);
* VSG003 no function pointers (Classic Coder, readable code).

Comments and string literals are ignored. Exit status: 0 = clean or no generated code,
1 = findings.
"""

from __future__ import annotations

import argparse
import re
import sys
from collections.abc import Sequence
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.vs import vs_common as vc

RULES: tuple[tuple[str, re.Pattern[str], str], ...] = (
    ("VSG001", re.compile(r"\b(malloc|calloc|realloc|free)\s*\("), "heap function"),
    ("VSG002", re.compile(r"\bva_arg\b"), "va_arg"),
    ("VSG003", re.compile(r"\(\s*\*\s*\w*\s*\)\s*\("), "function pointer"),
)
COMMENT_OR_STRING = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', re.S)


def strip_comments(text: str) -> str:
    """Replace comments and string literals by blanks, keeping line breaks."""
    return COMMENT_OR_STRING.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), text)


def check_file(path: Path, rel: str) -> list[str]:
    """Return the findings of one generated file."""
    findings = []
    lines = strip_comments(path.read_text(encoding="utf-8", errors="replace")).splitlines()
    for number, line in enumerate(lines, start=1):
        for rule, pattern, what in RULES:
            if pattern.search(line):
                findings.append(f"{rel}:{number}: {rule}: {what}")
    return findings


def main(argv: Sequence[str] | None = None) -> int:
    """Command-line entry point."""
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=vc.REPO_ROOT, help="repository root")
    parser.add_argument(
        "--variant", default="release", choices=vc.VARIANTS, help="variant to check"
    )
    args = parser.parse_args(argv)
    files = [p for p in vc.generated_files(args.root, args.variant) if p.suffix in vc.GEN_SUFFIXES]
    if not files:
        print(f"check_vs_gen: no generated code in gen_vs/{args.variant}")
        return 0
    findings: list[str] = []
    for path in files:
        findings += check_file(path, path.relative_to(args.root).as_posix())
    for finding in findings:
        print(finding)
    print(f"check_vs_gen: {len(files)} file(s), {len(findings)} finding(s)")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
