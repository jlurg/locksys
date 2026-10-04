# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Fail when a file contains bytes outside 7-bit ASCII (pre-commit hook dbc-ascii).

The CAN database must stay ASCII: CANdb++ and other DBC tools read it as cp1252.
Standard library only, so the hook runs with any Python 3.

Usage: python3 tools/codegen/check_ascii.py [--strict] FILE...
    --strict  also reject tab and carriage-return characters (LF line endings only)
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def problems(path: Path, strict: bool = False) -> list[str]:
    """Return one message per offending character of ``path``."""
    try:
        data = path.read_bytes()
    except OSError as exc:
        return [f"{path}: cannot read: {exc}"]
    found = []
    for number, line in enumerate(data.split(b"\n"), start=1):
        for column, byte in enumerate(line, start=1):
            if byte > 0x7F:
                found.append(f"{path}:{number}:{column}: non-ASCII byte 0x{byte:02X}")
            elif strict and byte in (0x09, 0x0D):
                name = "tab" if byte == 0x09 else "carriage return"
                found.append(f"{path}:{number}:{column}: {name}")
    return found


def main(argv: list[str] | None = None) -> int:
    """Command-line entry point."""
    parser = argparse.ArgumentParser(description="Reject non-ASCII bytes in files.")
    parser.add_argument("--strict", action="store_true", help="also reject tabs and CR")
    parser.add_argument("files", nargs="*", type=Path)
    args = parser.parse_args(argv)
    found = [message for path in args.files for message in problems(path, args.strict)]
    for message in found:
        print(message, file=sys.stderr)
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
