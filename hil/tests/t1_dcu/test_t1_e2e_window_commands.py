# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""T1 smoke: E2E protection of window commands (LS-HIL-002 TST-HIL-SYS-010)."""

import pytest

from locksys_hil.bench import Bench

pytestmark = [pytest.mark.hil_sim, pytest.mark.topology("T1")]


@pytest.mark.smoke
@pytest.mark.regression
@pytest.mark.test_id("TST-HIL-SYS-010")
@pytest.mark.verifies("SYS-037")
def test_e2e_protection_of_window_commands(topology: str, safe_bench: Bench) -> None:
    """Objective: corrupted, repeated and out-of-sequence CGW_WinCmd frames never move the window.

    Preconditions: T1 restbus as CGW; DCU NORMAL; stimulus plant model active.
    Steps: start a press through the restbus; inject CRC errors, a repeated counter and a counter
    jump; stop the injections.
    Expected: drive off within `lim_e2e_stop_ms`; no restart without a new press.
    """
    pytest.skip("catalogue body implemented in M5")
