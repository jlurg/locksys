# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Requirement identifiers for the collection checks and ``trace.json`` records.

The identifier formats are those of LS-VER-001 section 10.1. The registry is read from the
first column of the Markdown tables in ``docs/`` (the same rule as ``tools/trace/trace.py``).
"""

import json
import re
from collections.abc import Iterable
from dataclasses import dataclass
from pathlib import Path
from typing import Final

ID_RE: Final = re.compile(
    r"^(?:(?:STK|SYS|HWR|CSR)-\d{3}"
    r"|SWR-(?:DCU|CGW|APP|LIB)-\d{3}"
    r"|(?:HAZ|SG|SM|AS|TS|CSG)-\d{2}"
    r"|TST-(?:UT|IT|HIL|MAN)-(?:DCU|CGW|APP|SYS|LIB)-\d{3})$"
)
TEST_ID_RE: Final = re.compile(r"^TST-(?:UT|IT|HIL|MAN)-(?:DCU|CGW|APP|SYS|LIB)-\d{3}$")
_ROW_RE: Final = re.compile(r"^\|\s*`?([A-Z][A-Z0-9-]+)`?\s*\|")
_FENCE_RE: Final = re.compile(r"^\s*(```|~~~)")


def is_valid_id(identifier: str) -> bool:
    """True when ``identifier`` has a valid requirement, analysis or test format."""
    return ID_RE.match(identifier) is not None


def load_registry(docs_dir: Path) -> frozenset[str]:
    """Return every identifier found in the first column of a Markdown table under ``docs_dir``."""
    found: set[str] = set()
    for path in sorted(docs_dir.rglob("*.md")):
        in_fence = False
        for line in path.read_text(encoding="utf-8").splitlines():
            if _FENCE_RE.match(line):
                in_fence = not in_fence
                continue
            if in_fence:
                continue
            match = _ROW_RE.match(line)
            if match and is_valid_id(match.group(1)):
                found.add(match.group(1))
    return frozenset(found)


@dataclass(frozen=True)
class TraceRecord:
    """Identifiers and outcome of one executed test."""

    nodeid: str
    test_id: str | None
    verifies: tuple[str, ...]
    outcome: str


def write_trace_json(path: Path, records: Iterable[TraceRecord]) -> Path:
    """Write ``trace.json`` for ``tools/trace/trace.py``."""
    data = [
        {
            "nodeid": r.nodeid,
            "test_id": r.test_id,
            "verifies": list(r.verifies),
            "outcome": r.outcome,
        }
        for r in records
    ]
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    return path
