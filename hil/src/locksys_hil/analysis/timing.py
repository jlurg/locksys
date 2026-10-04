# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Timing statistics and the decision rule of LS-HIL-001 section 8.4.

A result passes only if the maximum over all trials plus the measurement uncertainty is within
the limit; a maximum above 80 % of the limit is flagged as low margin.
"""

import math
from collections.abc import Sequence
from dataclasses import dataclass

LOW_MARGIN_FRACTION = 0.8


def logic_uncertainty_s(duration_s: float, sample_rate_hz: float) -> float:
    """Method M1 uncertainty: two quantisation steps plus the 50 ppm timebase error."""
    return 2.0 / sample_rate_hz + 5e-5 * duration_s


def percentile(values: Sequence[float], fraction: float) -> float:
    """Nearest-rank percentile (fraction in 0..1).

    Raises:
        ValueError: no values or fraction outside 0..1.
    """
    if not values or not 0.0 <= fraction <= 1.0:
        raise ValueError("percentile needs values and a fraction in 0..1")
    ordered = sorted(values)
    rank = max(1, math.ceil(fraction * len(ordered)))
    return ordered[rank - 1]


@dataclass(frozen=True)
class Verdict:
    """Outcome of a timing criterion."""

    passed: bool
    low_margin: bool
    maximum: float
    p50: float
    p95: float
    p99: float
    limit: float
    uncertainty: float


def judge(values: Sequence[float], limit: float, uncertainty: float) -> Verdict:
    """Apply the guard-band rule to all trials (no retries, every trial counts)."""
    maximum = max(values)
    return Verdict(
        passed=maximum + uncertainty <= limit,
        low_margin=maximum > LOW_MARGIN_FRACTION * limit,
        maximum=maximum,
        p50=percentile(values, 0.50),
        p95=percentile(values, 0.95),
        p99=percentile(values, 0.99),
        limit=limit,
        uncertainty=uncertainty,
    )
