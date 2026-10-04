# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Run manifest (LS-HIL-001 section 11.4): identities and versions of everything in a run."""

import hashlib
import json
import platform
import sys
from dataclasses import asdict, dataclass, field
from datetime import UTC, datetime
from pathlib import Path

from locksys_hil import __version__


@dataclass
class RunManifest:
    """Content of ``manifest.json``; DUT and instrument entries are added by the fixtures."""

    run_id: str
    started_utc: str = field(default_factory=lambda: datetime.now(UTC).isoformat())
    framework_version: str = __version__
    python: str = sys.version.split()[0]
    os: str = field(default_factory=platform.platform)
    bench_config_sha256: str | None = None
    lock_file_sha256: str | None = None
    duts: dict[str, dict[str, str]] = field(default_factory=dict)
    instruments: dict[str, str] = field(default_factory=dict)
    software: dict[str, str] = field(default_factory=dict)

    def record_file_hash(self, attribute: str, path: Path) -> None:
        """Store the SHA-256 of ``path`` in ``attribute`` (e.g. ``bench_config_sha256``)."""
        setattr(self, attribute, hashlib.sha256(path.read_bytes()).hexdigest())

    def write(self, path: Path) -> Path:
        """Write the manifest as JSON."""
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(asdict(self), indent=2, sort_keys=True) + "\n", encoding="utf-8")
        return path
