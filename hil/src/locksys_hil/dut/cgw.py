# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""CGW flash adapter: esptool 5.4.0 Python API with addresses from ``flasher_args.json``.

eFuse and secure-boot operations are never performed by the framework (LS-SAIC-001 section 12).
"""

import json
from dataclasses import dataclass
from pathlib import Path

from locksys_hil.errors import BenchInfrastructureError


@dataclass(frozen=True)
class FlashSegment:
    """One image segment at its flash offset."""

    offset: int
    path: Path


def flash_segments(build_dir: Path) -> list[FlashSegment]:
    """Read the segments of an ESP-IDF build from ``flasher_args.json``.

    Raises:
        BenchInfrastructureError: the file is missing or malformed.
    """
    args_file = build_dir / "flasher_args.json"
    try:
        data = json.loads(args_file.read_text(encoding="utf-8"))
        files: dict[str, str] = data["flash_files"]
    except (OSError, KeyError, ValueError) as exc:
        raise BenchInfrastructureError(f"cannot read {args_file}: {exc}") from exc
    return sorted(
        (FlashSegment(int(offset, 16), build_dir / name) for offset, name in files.items()),
        key=lambda seg: seg.offset,
    )


def flash(port: str, segments: list[FlashSegment]) -> None:
    """Write and verify the segments, then hard-reset the chip.

    Raises:
        BenchInfrastructureError: any esptool step fails.
    """
    from esptool.cmds import (
        attach_flash,
        detect_chip,
        reset_chip,
        run_stub,
        verify_flash,
        write_flash,
    )

    addr_files = [(seg.offset, seg.path.open("rb")) for seg in segments]
    try:
        with detect_chip(port) as esp:
            esp = run_stub(esp)
            attach_flash(esp)
            write_flash(esp, addr_files)
            verify_flash(esp, addr_files)
            reset_chip(esp, "hard-reset")
    except Exception as exc:  # esptool raises several unrelated types
        raise BenchInfrastructureError(f"CGW flashing failed: {exc}") from exc
    finally:
        for _, stream in addr_files:
            stream.close()
