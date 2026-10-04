# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Console redaction and evidence secret scan."""

import base64
from pathlib import Path

from locksys_hil.security.redact import (
    SECRET_BEGIN,
    SECRET_END,
    ConsoleRedactor,
    scan_for_secrets,
    secret_variants,
)


def test_secret_block_and_payload_are_redacted() -> None:
    lines = [
        "I (100) cgw: pairing window open",
        SECRET_BEGIN,
        "    ████  ",
        "locksys://pair?v=1&id=0102030405060708&p=ABCDEF&k=xyz",
        SECRET_END,
        "W (200) cgw: p=SECRET k=KEY other=1",
        "visit locksys://pair?v=1&k=abc now",
    ]
    out = list(ConsoleRedactor().redact(lines))
    assert out[0] == lines[0]
    assert out[1] == "<redacted 2 lines>"
    assert out[2] == "W (200) cgw: p=<redacted> k=<redacted> other=1"
    assert out[3] == "visit <redacted> now"
    assert not any("ABCDEF" in line or "KEY" in line for line in out)


def test_scan_finds_all_encodings(tmp_path: Path) -> None:
    key = bytes(range(32))
    assert len(secret_variants(key)) >= 5
    clean = tmp_path / "clean.log"
    clean.write_text("nothing here\n", encoding="utf-8")
    leaked = tmp_path / "leak.log"
    leaked.write_bytes(b"k=" + base64.urlsafe_b64encode(key).rstrip(b"="))
    marker = tmp_path / "marker.log"
    marker.write_text(SECRET_BEGIN, encoding="utf-8")
    assert scan_for_secrets([clean, leaked, marker, tmp_path], [key]) == [leaked, marker]
