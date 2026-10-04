# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Redaction of CGW console output and secret scanning of evidence files.

The pairing block between ``<<LS-SECRET-BEGIN>>`` and ``<<LS-SECRET-END>>`` becomes
``<redacted N lines>``; ``locksys://pair`` payloads and ``p=``/``k=`` parameters are masked
(LS-SAIC-001 section 8.5 item 10).
"""

import base64
import re
from collections.abc import Iterable, Iterator
from dataclasses import dataclass, field
from pathlib import Path
from typing import Final

SECRET_BEGIN: Final = "<<LS-SECRET-BEGIN>>"
SECRET_END: Final = "<<LS-SECRET-END>>"
MASK: Final = "<redacted>"
_PAIR_URI: Final = re.compile(r"locksys://pair\S*")
_PARAM: Final = re.compile(r"(?<![A-Za-z0-9_])([pk])=[^&\s]+")


@dataclass
class ConsoleRedactor:
    """Line filter applied to every console line before any sink."""

    _in_block: bool = field(default=False, init=False)
    _block_lines: int = field(default=0, init=False)

    def feed(self, line: str) -> str | None:
        """Return the redacted line, or None while inside a secret block."""
        if self._in_block:
            if SECRET_END in line:
                self._in_block = False
                return f"<redacted {self._block_lines} lines>"
            self._block_lines += 1
            return None
        if SECRET_BEGIN in line:
            self._in_block = True
            self._block_lines = 0
            return None
        return _PARAM.sub(rf"\1={MASK}", _PAIR_URI.sub(MASK, line))

    def redact(self, lines: Iterable[str]) -> Iterator[str]:
        """Redact a sequence of lines."""
        for line in lines:
            out = self.feed(line)
            if out is not None:
                yield out


def secret_variants(secret: bytes) -> list[bytes]:
    """Return the encodings searched for one secret: raw, hex, base64 and base64url."""
    b64 = base64.b64encode(secret)
    b64url = base64.urlsafe_b64encode(secret)
    variants = {
        secret,
        secret.hex().encode(),
        secret.hex().upper().encode(),
        b64,
        b64.rstrip(b"="),
        b64url,
        b64url.rstrip(b"="),
    }
    return sorted(v for v in variants if v)


def scan_for_secrets(paths: Iterable[Path], secrets: Iterable[bytes]) -> list[Path]:
    """Return the files that contain any encoding of any secret or a pairing marker."""
    needles = [v for s in secrets for v in secret_variants(s)]
    needles += [SECRET_BEGIN.encode(), b"locksys://pair"]
    hits: list[Path] = []
    for path in paths:
        if not path.is_file():
            continue
        data = path.read_bytes()
        if any(needle in data for needle in needles):
            hits.append(path)
    return hits
