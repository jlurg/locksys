# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Edge extraction from sampled digital channels and VNH5019 drive decoding.

Drive-on: INA != INB and PWM high. Brake: INA = INB = 0 and PWM high. The drive-off instant is
the first of INA = INB, or PWM low for at least 100 us (LS-HIL-001 section 8.4).
"""

from collections.abc import Sequence
from dataclasses import dataclass
from enum import StrEnum


class Edge(StrEnum):
    """Edge polarity."""

    RISING = "rising"
    FALLING = "falling"


@dataclass(frozen=True)
class Transition:
    """One level change of a channel at time ``t_s``."""

    t_s: float
    edge: Edge


def edges(times_s: Sequence[float], levels: Sequence[int]) -> list[Transition]:
    """Return the transitions of a channel given as (time, level) samples.

    Raises:
        ValueError: the sequences differ in length.
    """
    if len(times_s) != len(levels):
        raise ValueError("times and levels differ in length")
    out: list[Transition] = []
    for i in range(1, len(levels)):
        if levels[i] != levels[i - 1]:
            out.append(Transition(times_s[i], Edge.RISING if levels[i] else Edge.FALLING))
    return out


class DriveState(StrEnum):
    """Decoded H-bridge state."""

    OFF = "off"
    UP = "up"
    DOWN = "down"
    BRAKE = "brake"


def drive_state(ina: int, inb: int, pwm: int) -> DriveState:
    """Decode one sample of INA, INB and PWM (direction convention: INA high = UP)."""
    if not pwm:
        return DriveState.OFF
    if ina and not inb:
        return DriveState.UP
    if inb and not ina:
        return DriveState.DOWN
    return DriveState.BRAKE


def first_drive_off(
    times_s: Sequence[float],
    ina: Sequence[int],
    inb: Sequence[int],
    pwm: Sequence[int],
    after_s: float,
    pwm_low_min_s: float = 100e-6,
) -> float | None:
    """Return the first drive-off instant at or after ``after_s``, or None."""
    low_since: float | None = None
    for t, a, b, p in zip(times_s, ina, inb, pwm, strict=True):
        if t < after_s:
            continue
        if a == b:
            return t
        if p:
            low_since = None
            continue
        if low_since is None:
            low_since = t
        if t - low_since >= pwm_low_min_s:
            return low_since
    return None
