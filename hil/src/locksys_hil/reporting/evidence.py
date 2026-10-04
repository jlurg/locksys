# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Evidence directory layout: ``<evidence_dir>/<run-id>/evidence/<test-id>/``."""

import re
from dataclasses import dataclass
from pathlib import Path

_UNSAFE = re.compile(r"[^A-Za-z0-9_.-]+")


@dataclass(frozen=True)
class EvidenceStore:
    """Evidence root of one run."""

    root: Path

    def run_dir(self) -> Path:
        """Return (and create) the run directory."""
        self.root.mkdir(parents=True, exist_ok=True)
        return self.root

    def test_dir(self, test_id: str) -> Path:
        """Return (and create) the evidence directory of one test."""
        path = self.root / "evidence" / _UNSAFE.sub("_", test_id)
        path.mkdir(parents=True, exist_ok=True)
        return path
