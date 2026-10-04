# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Firmware image verification against the CI build manifest (SHA-256)."""

import hashlib
from pathlib import Path

from locksys_hil.errors import BenchInfrastructureError


def sha256_file(path: Path) -> str:
    """Return the lowercase hexadecimal SHA-256 of a file."""
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 16), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify_image(path: Path, expected_sha256: str) -> None:
    """Check an image before it is flashed.

    Raises:
        BenchInfrastructureError: the image is missing or its hash differs from the manifest.
    """
    if not path.is_file():
        raise BenchInfrastructureError(f"image not found: {path}")
    actual = sha256_file(path)
    if actual != expected_sha256.lower():
        raise BenchInfrastructureError(f"SHA-256 mismatch for {path.name}: {actual}")
