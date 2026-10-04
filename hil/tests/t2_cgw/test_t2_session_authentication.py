# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""T2 smoke: session authentication, replay and throttling (LS-HIL-002 TST-HIL-SYS-018)."""

import pytest

from locksys_hil.bench import Bench

pytestmark = [pytest.mark.hil_sim, pytest.mark.topology("T2")]


@pytest.mark.smoke
@pytest.mark.regression
@pytest.mark.test_id("TST-HIL-SYS-018")
@pytest.mark.verifies("SYS-050", "SYS-051", "SYS-056")
def test_session_authentication(topology: str, safe_bench: Bench) -> None:
    """Objective: only authenticated, fresh frames reach the arbiter.

    Preconditions: T2 restbus as DCU; CGW paired with the bench key; APP simulator associated.
    Steps: authenticate; replay a frame; send a frame with a wrong tag; authenticate with a wrong
    key three times.
    Expected: replay and wrong tag close with 1008 and latch STOP; the fourth attempt within
    `t_auth_throttle_ms` is answered with REJECTED_RATE_LIMIT and close 4006.
    """
    pytest.skip("catalogue body implemented in M5")
