# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""T3 smoke: hold-to-run, motion only while held (LS-HIL-002 TST-HIL-SYS-001)."""

import pytest

from locksys_hil.bench import Bench

pytestmark = [pytest.mark.hil_sim, pytest.mark.topology("T3")]


@pytest.mark.smoke
@pytest.mark.regression
@pytest.mark.test_id("TST-HIL-SYS-001")
@pytest.mark.verifies("SYS-005")
def test_hold_to_run(topology: str, safe_bench: Bench) -> None:
    """Objective: the window moves only while the APP simulator holds a press.

    Preconditions: T3; DCU and CGW NORMAL; authenticated APP simulator session.
    Steps: press UP with keep-alive for 2 s; release; repeat DOWN.
    Expected: drive on only during the press; drive off within `lim_release_stop_appsim_ms`
    after WindowStop.
    """
    pytest.skip("catalogue body implemented in M5")
