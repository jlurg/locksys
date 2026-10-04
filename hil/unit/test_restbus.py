# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Restbus schedule, E2E counters and fault hooks."""

import time

from locksys_hil.instruments.fakes import FakeCanBus
from locksys_hil.protocols.com import ComCodec
from locksys_hil.protocols.e2e import E2eState, protect
from locksys_hil.protocols.restbus import Restbus, RestbusRole, RestbusRunner


def test_cgw_role_schedule_and_e2e() -> None:
    codec = ComCodec()
    restbus = Restbus(codec, RestbusRole.CGW)
    assert set(restbus.messages) == {"CGW_WinCmd", "CGW_NodeSts", "CGW_Version"}
    receiver = codec.receiver("CGW_WinCmd")
    win_id = codec.frame_id("CGW_WinCmd")
    sent = [f for now in range(100) for f in restbus.due(now) if f.arbitration_id == win_id]
    assert len(sent) == 5
    results = [receiver.receive(frame.data) for frame in sent]
    assert results[-1].state is E2eState.VALID
    assert all(r.status == "OK" for r in results)


def test_fault_hook_corrupts_and_suppresses() -> None:
    codec = ComCodec()
    restbus = Restbus(codec, RestbusRole.DCU)
    restbus.hooks["DCU_WinSts"] = lambda _name, data: bytes((data[0] ^ 0xFF, *data[1:]))
    restbus.hooks["DCU_NodeSts"] = lambda _name, _data: None
    frames = restbus.due(0)
    ids = {f.arbitration_id for f in frames}
    assert codec.frame_id("DCU_NodeSts") not in ids
    win = next(f for f in frames if f.arbitration_id == codec.frame_id("DCU_WinSts"))
    assert not codec.receiver("DCU_WinSts").receive(win.data).delivered
    assert protect(win.data, 0x2001, 0)[0] != win.data[0]


def test_runner_sends_on_bus() -> None:
    codec = ComCodec()
    restbus = Restbus(codec, RestbusRole.CGW)
    restbus.set_signals("CGW_WinCmd", {"WinCmd_PressId": 7})
    bus = FakeCanBus()
    runner = RestbusRunner(restbus, bus)
    runner.start()
    runner.start()
    time.sleep(0.1)
    runner.stop()
    assert any(f.arbitration_id == codec.frame_id("CGW_WinCmd") for f in bus.sent)
