# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Marker catalogue (LS-HIL-001 section 9.3) and marker argument helpers."""

from typing import Final

import pytest

MARKERS: Final[tuple[str, ...]] = (
    "smoke: smoke suite (develop push), <= 15 min",
    "regression: regression suite (release and hotfix branches)",
    "nightly: nightly suite (schedule)",
    "soak: endurance suite (weekly, release candidates)",
    "manual: operator-assisted procedure; never run unattended",
    "hil_sim: HIL-SIM configuration: plant models on the stimulus MCU, unattended",
    "hil_real: HIL-REAL configuration: real actuators, attended only",
    "dev_build: needs a DEV or HIL build (fault injection or trace pins)",
    "destructive: changes non-volatile DUT state (option bytes, NVS, keys)",
    "requires(*capabilities): bench capabilities needed; skipped when missing",
    "test_id(id): catalogue identifier TST-<UT|IT|HIL|MAN>-<node>-nnn",
    "verifies(*ids): requirement or mechanism identifiers verified by the test",
    "topology(name): integration topology T1, T2 or T3 (LS-HIL-001 section 4)",
    "quarantine(issue, until): reported but not gating; at most 14 days",
)
"""Marker ini lines (``name[(signature)]: description``)."""

CATALOGUE_MARKERS: Final = frozenset({"hil_sim", "hil_real", "manual"})
"""Tests with one of these markers are catalogue tests and need test_id and verifies."""


def ini_lines() -> list[str]:
    """Return the ``markers`` ini lines."""
    return list(MARKERS)


def marker_args(item: pytest.Item, name: str) -> tuple[str, ...]:
    """Return the string arguments of all ``name`` markers of ``item`` (outermost first)."""
    values: list[str] = []
    for mark in reversed(list(item.iter_markers(name))):
        values.extend(str(arg) for arg in mark.args)
    return tuple(values)
