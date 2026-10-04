# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Restbus simulation of the CGW or DCU role (LS-HIL-001 section 4, topologies T1 and T2).

The scheduler is pure (``due``) so that it is testable without a bus; ``RestbusRunner`` drives
it from a thread over a ``CanBus`` driver. Fault hooks may change or suppress each frame after
E2E protection (CRC corruption, counter repetition, silence).
"""

import threading
import time
from collections.abc import Callable, Mapping
from dataclasses import dataclass, field
from enum import StrEnum

from locksys_hil.instruments.base import CanBus, CanFrame
from locksys_hil.protocols.com import ComCodec
from locksys_hil.protocols.e2e import COUNTER_MODULO

FaultHook = Callable[[str, bytes], bytes | None]
"""Hook called with (message name, protected frame); returns the frame to send or None."""


class RestbusRole(StrEnum):
    """Node simulated by the restbus."""

    CGW = "CGW"
    DCU = "DCU"


@dataclass
class _Slot:
    name: str
    frame_id: int
    period_ms: int
    next_ms: int
    counter: int = 0


@dataclass
class Restbus:
    """Cyclic transmission schedule of one simulated node.

    Attributes:
        codec: CAN codec.
        role: Simulated node; its cyclic messages are scheduled.
        signals: Current raw signal values per message.
        hooks: Fault hooks per message name.
    """

    codec: ComCodec
    role: RestbusRole
    signals: dict[str, dict[str, int]] = field(default_factory=dict)
    hooks: dict[str, FaultHook] = field(default_factory=dict)
    _slots: list[_Slot] = field(default_factory=list, init=False)

    def __post_init__(self) -> None:
        for name in self.codec.senders(self.role.value):
            period = self.codec.cycle_time_ms(name)
            if period > 0:
                self._slots.append(_Slot(name, self.codec.frame_id(name), period, 0))

    @property
    def messages(self) -> list[str]:
        """Names of the scheduled cyclic messages."""
        return [slot.name for slot in self._slots]

    def set_signals(self, name: str, values: Mapping[str, int]) -> None:
        """Update raw signal values of ``name`` for the next transmissions."""
        self.signals.setdefault(name, {}).update(values)

    def due(self, now_ms: int) -> list[CanFrame]:
        """Return the frames due at ``now_ms`` and advance their schedules and counters."""
        frames: list[CanFrame] = []
        for slot in self._slots:
            if now_ms < slot.next_ms:
                continue
            slot.next_ms = now_ms + slot.period_ms
            data = self.codec.encode_raw(slot.name, self.signals.get(slot.name, {}), slot.counter)
            slot.counter = (slot.counter + 1) % COUNTER_MODULO
            hook = self.hooks.get(slot.name)
            out = hook(slot.name, data) if hook is not None else data
            if out is not None:
                frames.append(CanFrame(slot.frame_id, out))
        return frames


class RestbusRunner:
    """Run a ``Restbus`` on a ``CanBus`` from a background thread (1 ms resolution)."""

    def __init__(self, restbus: Restbus, bus: CanBus) -> None:
        """Bind the schedule to a bus."""
        self._restbus = restbus
        self._bus = bus
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None

    def start(self) -> None:
        """Start transmitting."""
        if self._thread is not None:
            return
        self._stop.clear()
        self._thread = threading.Thread(target=self._run, name="restbus", daemon=True)
        self._thread.start()

    def stop(self) -> None:
        """Stop transmitting and wait for the thread."""
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=1.0)
            self._thread = None

    def _run(self) -> None:
        start = time.perf_counter()
        while not self._stop.is_set():
            now_ms = int((time.perf_counter() - start) * 1000)
            for frame in self._restbus.due(now_ms):
                self._bus.send(frame)
            self._stop.wait(0.001)
