# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""APP simulator behaviour library (LS-HIL-001 section 9.1).

Each behaviour is a stimulus pattern against the CGW session supervision. M0 defines the
catalogue; the behaviours are implemented with the HIL catalogue tests (M5).
"""

from enum import StrEnum


class Behaviour(StrEnum):
    """Stimulus patterns of the APP simulator."""

    NOMINAL = "nominal"
    """Keep-alive every ``t_app_ka_ms`` and an explicit WindowStop on release."""
    FREEZE = "freeze"
    """Socket stays open, no frames are sent (keep-alive loss)."""
    KILL = "kill"
    """TCP connection is dropped without a close frame."""
    NO_STOP = "no_stop"
    """Keep-alive stops without WindowStop."""
    FLOOD = "flood"
    """More than ``n_rate_limit_frames`` frames per ``t_rate_window_ms``."""
    FUZZ = "fuzz"
    """Seeded malformed frames."""
    REPLAY = "replay"
    """A previously sent frame is sent again."""
    WRONG_KEY = "wrong_key"
    """ClientAuth with a proof from another K_pair."""
    WRONG_VERSION = "wrong_version"
    """Protocol major version 2 or a wrong subprotocol."""
    SLOW_PONG = "slow_pong"
    """Pong delayed beyond ``t_rtt_max_ms``."""
    SECOND_CLIENT = "second_client"
    """A second client authenticates while the controller session is alive."""
