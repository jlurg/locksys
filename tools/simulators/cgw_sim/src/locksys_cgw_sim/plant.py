# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""DCU-side plant model of the simulator: door lock, free-spinning window motor, temperature."""

import random
from dataclasses import dataclass, field

from locksys_hil.gen import locksys_app_pb2 as pb
from locksys_hil.gen import params

WINDOW_FREE_SPEED_RPM_X10 = 1700
"""Free-running output speed of the JGB37-520 1:60 motor at 12 V [0.1 rpm]."""


@dataclass
class DoorResult:
    """Completion of a door transaction."""

    result: pb.CommandResult
    lock_state: pb.DoorLockState


@dataclass
class Plant:
    """State of the simulated DCU.

    Attributes:
        lock_state: DoorLockState.
        window_state: WindowState.
        stop_reason: WindowStopReason of the last stop.
        temperature_cdeg: Temperature in cdeg.
        dcu_alive: False simulates a lost DCU (DCU_NodeSts timeout).
        dcu_mode: NodeMode of the DCU.
    """

    lock_state: pb.DoorLockState = pb.DOOR_LOCK_STATE_LOCKED
    window_state: pb.WindowState = pb.WINDOW_STATE_STOPPED
    stop_reason: pb.WindowStopReason = pb.WINDOW_STOP_REASON_NONE
    temperature_cdeg: int = 2345
    temp_status: pb.TempStatus = pb.TEMP_STATUS_VALID
    dcu_alive: bool = True
    dcu_mode: pb.NodeMode = pb.NODE_MODE_NORMAL
    vbat_dv: int = 124
    seed: int = 1
    _door_target: pb.DoorLockState | None = field(default=None, init=False)
    _door_done_ms: int = field(default=0, init=False)
    _rng: random.Random = field(init=False, repr=False)
    _last_temp_ms: int = field(default=0, init=False)

    def __post_init__(self) -> None:
        self._rng = random.Random(self.seed)

    @property
    def moving(self) -> bool:
        """True while the window motor runs."""
        return self.window_state in (pb.WINDOW_STATE_MOVING_UP, pb.WINDOW_STATE_MOVING_DOWN)

    @property
    def speed_rpm_x10(self) -> int:
        """Signed window motor speed."""
        if self.window_state == pb.WINDOW_STATE_MOVING_UP:
            return WINDOW_FREE_SPEED_RPM_X10
        if self.window_state == pb.WINDOW_STATE_MOVING_DOWN:
            return -WINDOW_FREE_SPEED_RPM_X10
        return 0

    @property
    def door_busy(self) -> bool:
        """True while a lock pulse is in progress."""
        return self._door_target is not None

    def start_window(self, direction: pb.WindowDirection) -> None:
        """Start the window motor UP or DOWN (stage A: no end positions)."""
        self.window_state = (
            pb.WINDOW_STATE_MOVING_UP
            if direction == pb.WINDOW_DIRECTION_UP
            else pb.WINDOW_STATE_MOVING_DOWN
        )
        self.stop_reason = pb.WINDOW_STOP_REASON_NONE

    def stop_window(self, reason: pb.WindowStopReason) -> None:
        """Stop the window motor with a stop reason."""
        if self.moving:
            self.window_state = pb.WINDOW_STATE_STOPPED
            self.stop_reason = reason

    def request_door(self, action: pb.DoorAction, now_ms: int) -> DoorResult | None:
        """Start a lock pulse; returns an immediate result when the switch already shows the state."""
        target = (
            pb.DOOR_LOCK_STATE_LOCKED
            if action == pb.DOOR_ACTION_LOCK
            else pb.DOOR_LOCK_STATE_UNLOCKED
        )
        if self.lock_state == target:
            return DoorResult(pb.COMMAND_RESULT_OK, target)
        self._door_target = target
        self._door_done_ms = now_ms + params.T_LOCK_PULSE_MS + params.T_LOCK_SETTLE_MS
        self.lock_state = (
            pb.DOOR_LOCK_STATE_LOCKING
            if target == pb.DOOR_LOCK_STATE_LOCKED
            else pb.DOOR_LOCK_STATE_UNLOCKING
        )
        return None

    def tick(self, now_ms: int) -> DoorResult | None:
        """Advance the plant; returns the result of a lock pulse that completed."""
        if now_ms - self._last_temp_ms >= params.T_TEMP_PERIOD_MS:
            self._last_temp_ms = now_ms
            step = self._rng.choice((-2, -1, 0, 0, 1, 2))
            self.temperature_cdeg = max(-4000, min(12500, self.temperature_cdeg + step))
        if self._door_target is not None and now_ms >= self._door_done_ms:
            self.lock_state = self._door_target
            self._door_target = None
            return DoorResult(pb.COMMAND_RESULT_OK, self.lock_state)
        return None
