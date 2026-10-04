# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Edge extraction, drive decoding and the timing decision rule."""

import pytest

from locksys_hil.analysis.edges import DriveState, Edge, drive_state, edges, first_drive_off
from locksys_hil.analysis.timing import judge, logic_uncertainty_s, percentile


def test_edges() -> None:
    result = edges([0.0, 1.0, 2.0, 3.0], [0, 1, 1, 0])
    assert [(t.t_s, t.edge) for t in result] == [(1.0, Edge.RISING), (3.0, Edge.FALLING)]
    with pytest.raises(ValueError, match="differ"):
        edges([0.0], [0, 1])


def test_drive_state() -> None:
    assert drive_state(1, 0, 1) is DriveState.UP
    assert drive_state(0, 1, 1) is DriveState.DOWN
    assert drive_state(0, 0, 1) is DriveState.BRAKE
    assert drive_state(1, 1, 1) is DriveState.BRAKE
    assert drive_state(1, 0, 0) is DriveState.OFF


def test_first_drive_off() -> None:
    times = [i * 50e-6 for i in range(10)]
    ina = [1] * 10
    inb = [0] * 10
    pwm = [1, 1, 0, 0, 1, 0, 0, 0, 0, 0]  # 50 us low pulse is PWM, not drive-off
    assert first_drive_off(times, ina, inb, pwm, 0.0) == pytest.approx(250e-6)
    assert first_drive_off(
        times, ina, [0, 0, 0, 1, 0, 0, 0, 0, 0, 0], [1] * 10, 0.0
    ) == pytest.approx(150e-6)
    assert first_drive_off(times, ina, inb, [1] * 10, 0.0) is None


def test_judge_guard_band_and_margin() -> None:
    values = [90.0, 100.0, 110.0, 120.0]
    verdict = judge(values, limit=140.0, uncertainty=1.0)
    assert verdict.passed
    assert verdict.low_margin
    assert verdict.maximum == 120.0
    assert not judge(values, limit=120.5, uncertainty=1.0).passed
    assert percentile(values, 0.5) == 100.0
    assert logic_uncertainty_s(1.0, 25e6) == pytest.approx(80e-9 + 50e-6)
    with pytest.raises(ValueError, match="percentile"):
        percentile([], 0.5)
