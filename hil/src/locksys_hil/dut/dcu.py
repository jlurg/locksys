# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""DCU flash adapter: STM32CubeProgrammer CLI (STM32CubeCLT 1.19.0) over SWD.

The command line follows LS-HIL-001 section 9.1; the version check reads ``$LSVER`` and the
DCU_Version frame after reset (implemented with the HIL catalogue in M5).
"""

import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path

from locksys_hil.dut.artifacts import verify_image
from locksys_hil.errors import BenchInfrastructureError


@dataclass(frozen=True)
class DcuFlasher:
    """Flash a DCU image through its ST-LINK."""

    stlink_serial: str
    programmer: str = "STM32_Programmer_CLI"

    def command(self, image: Path, log: Path) -> list[str]:
        """Return the programmer command line for ``image``."""
        return [
            self.programmer,
            "-c",
            "port=SWD",
            f"sn={self.stlink_serial}",
            "freq=4000",
            "mode=UR",
            "reset=HWrst",
            "-w",
            str(image),
            "-v",
            "-hardRst",
            "-q",
            "-log",
            str(log),
        ]

    def flash(self, image: Path, sha256: str, log: Path, timeout_s: float = 120.0) -> None:
        """Verify and flash ``image``.

        Raises:
            BenchInfrastructureError: hash mismatch, programmer missing or programming failure.
        """
        verify_image(image, sha256)
        if shutil.which(self.programmer) is None:
            raise BenchInfrastructureError(f"{self.programmer} not found on PATH")
        result = subprocess.run(
            self.command(image, log), capture_output=True, text=True, timeout=timeout_s, check=False
        )
        if result.returncode != 0:
            raise BenchInfrastructureError(f"DCU programming failed ({result.returncode})")
