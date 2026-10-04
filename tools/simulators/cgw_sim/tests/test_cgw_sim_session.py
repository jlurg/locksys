# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Simulator session engine against ``interfaces/vectors/app_session_v1.json``."""

import json
from collections.abc import Callable
from pathlib import Path
from typing import Any

import pytest

from locksys_cgw_sim.core import (
    CLOSE_AUTH,
    CLOSE_BUSY,
    CLOSE_POLICY,
    CLOSE_SESSION,
    CLOSE_THROTTLED,
    CLOSE_VERSION,
    CgwSimCore,
    Outputs,
    SimConfig,
)
from locksys_hil.gen import locksys_app_pb2 as pb
from locksys_hil.gen import params
from locksys_hil.protocols.appsim import crypto
from locksys_hil.protocols.appsim.codec import (
    SessionChannel,
    decode_handshake,
    encode_handshake,
    member,
)

VECTORS = Path(__file__).resolve().parents[4] / "interfaces" / "vectors" / "app_session_v1.json"
T0 = 1000


@pytest.fixture(scope="module")
def vectors() -> Any:
    return json.loads(VECTORS.read_text(encoding="utf-8"))


def _frame(vectors: Any, name: str) -> bytes:
    return bytes.fromhex(next(f for f in vectors["frames"] if f["name"] == name)["frame"])


def _random_source(nonce: bytes, session_id: bytes) -> Callable[[int], bytes]:
    """Serve the vector server nonce for every 16-byte request and the session ID for 4 bytes."""

    def source(size: int) -> bytes:
        return nonce if size == len(nonce) else session_id[:size]

    return source


def _core(vectors: Any) -> CgwSimCore:
    inputs = vectors["inputs"]
    config = SimConfig(
        k_pair=bytes.fromhex(inputs["k_pair"]),
        device_id=bytes.fromhex(inputs["device_id"]),
        fw_version="0.1.0-dev+a1b2c3d",
        random_bytes=_random_source(
            bytes.fromhex(inputs["server_nonce"]), (0x12345678).to_bytes(4, "big")
        ),
    )
    return CgwSimCore(config)


def _app(vectors: Any) -> SessionChannel:
    return SessionChannel(
        k_sess=bytes.fromhex(vectors["k_sess"]), tx_direction=crypto.Direction.APP_TO_CGW
    )


def _bodies(app: SessionChannel, outputs: Outputs, link: int) -> list[pb.Body]:
    out = outputs.get(link)
    return [app.open(frame) for frame in out.frames] if out is not None else []


def _authenticate(vectors: Any) -> tuple[CgwSimCore, int, SessionChannel]:
    core = _core(vectors)
    link, hello = core.connect(T0)
    assert hello.frames == [_frame(vectors, "server_hello")]
    out = core.receive(link, _frame(vectors, "client_auth"), T0)
    assert out[link].frames == [
        _frame(vectors, "auth_result_ok"),
        _frame(vectors, "ping_after_auth"),
    ]
    assert out[link].close_code is None
    return core, link, _app(vectors)


def _pong_and_press(core: CgwSimCore, link: int, app: SessionChannel, now: int) -> Outputs:
    core.receive(link, app.seal(pb.Body(pong=pb.Pong(echo_timestamp_ms=T0))), now)
    move = pb.WindowMove(press_id=1, direction=pb.WINDOW_DIRECTION_UP, hold_ms=0)
    return core.receive(link, app.seal(pb.Body(window_move=move)), now)


def test_handshake_reproduces_the_vectors(vectors: Any) -> None:
    _authenticate(vectors)


def test_press_is_acknowledged_with_the_vector_frame(vectors: Any) -> None:
    core, link, app = _authenticate(vectors)
    out = _pong_and_press(core, link, app, T0 + 20)
    assert out[link].frames == [_frame(vectors, "command_ack_accepted")]
    assert core.plant.window_state == pb.WINDOW_STATE_MOVING_UP


def test_keepalive_timeout_latches_stop(vectors: Any) -> None:
    core, link, app = _authenticate(vectors)
    app.rx_counter = 2
    _pong_and_press(core, link, app, T0 + 20)
    app.rx_counter = 3
    core.tick(T0 + 20 + params.T_CGW_KA_TO_MS)
    assert core.plant.window_state == pb.WINDOW_STATE_MOVING_UP
    bodies = _bodies(app, core.tick(T0 + 21 + params.T_CGW_KA_TO_MS), link)
    ack = next(b.command_ack for b in bodies if member(b) == "command_ack")
    assert (ack.ref_id, ack.result) == (1, pb.COMMAND_RESULT_FAILED_TIMEOUT)
    notice = next(b.notice for b in bodies if member(b) == "notice")
    assert (notice.code, notice.text) == (1, "Keep-alive timeout")
    assert core.plant.window_state == pb.WINDOW_STATE_STOPPED
    stale = pb.WindowMove(press_id=1, direction=pb.WINDOW_DIRECTION_UP, hold_ms=500)
    out = core.receive(link, app.seal(pb.Body(window_move=stale)), T0 + 400)
    assert out[link].frames == []
    assert core.plant.window_state == pb.WINDOW_STATE_STOPPED


def test_keepalive_and_release(vectors: Any) -> None:
    core, link, app = _authenticate(vectors)
    _pong_and_press(core, link, app, T0 + 20)
    for step in range(1, 6):
        now = T0 + 20 + step * params.T_APP_KA_MS
        move = pb.WindowMove(press_id=1, direction=pb.WINDOW_DIRECTION_UP, hold_ms=step * 100)
        core.receive(link, app.seal(pb.Body(window_move=move)), now)
        assert core.links[link].press is not None
    core.receive(link, app.seal(pb.Body(window_stop=pb.WindowStop(press_id=1))), T0 + 600)
    assert core.plant.window_state == pb.WINDOW_STATE_STOPPED
    assert core.plant.stop_reason == pb.WINDOW_STOP_REASON_RELEASED


def test_press_without_rtt_sample_is_rejected(vectors: Any) -> None:
    core, link, app = _authenticate(vectors)
    app.rx_counter = 2
    move = pb.WindowMove(press_id=1, direction=pb.WINDOW_DIRECTION_UP, hold_ms=0)
    out = core.receive(link, app.seal(pb.Body(window_move=move)), T0 + 10)
    (body,) = _bodies(app, out, link)
    assert body.command_ack.result == pb.COMMAND_RESULT_REJECTED_LINK_QUALITY


def test_door_transaction_and_cache(vectors: Any) -> None:
    core, link, app = _authenticate(vectors)
    app.rx_counter = 2
    unlock = pb.Body(door_command=pb.DoorCommand(request_id=1, action=pb.DOOR_ACTION_UNLOCK))
    (ack,) = _bodies(app, core.receive(link, app.seal(unlock), T0 + 10), link)
    assert ack.command_ack.result == pb.COMMAND_RESULT_ACCEPTED
    assert core.plant.lock_state == pb.DOOR_LOCK_STATE_UNLOCKING
    (again,) = _bodies(app, core.receive(link, app.seal(unlock), T0 + 20), link)
    assert again.command_ack.result == pb.COMMAND_RESULT_ACCEPTED
    done = T0 + 10 + params.T_LOCK_PULSE_MS + params.T_LOCK_SETTLE_MS
    bodies = _bodies(app, core.tick(done), link)
    result = next(b.door_command_result for b in bodies if member(b) == "door_command_result")
    assert (result.request_id, result.result, result.lock_state) == (
        1,
        pb.COMMAND_RESULT_OK,
        pb.DOOR_LOCK_STATE_UNLOCKED,
    )
    (cached,) = _bodies(app, core.receive(link, app.seal(unlock), done + 100), link)
    assert cached.door_command_result == result


def test_status_push_and_request(vectors: Any) -> None:
    core, link, app = _authenticate(vectors)
    app.rx_counter = 2
    bodies = _bodies(app, core.tick(T0 + 10), link)
    status = next(b.status_update for b in bodies if member(b) == "status_update")
    assert status.seq == 1
    assert status.dcu_alive
    assert status.door_lock_state == pb.DOOR_LOCK_STATE_LOCKED
    assert _bodies(app, core.tick(T0 + 20), link) == []
    core.receive(link, app.seal(pb.Body(status_request=pb.StatusRequest())), T0 + 30)
    bodies = _bodies(app, core.tick(T0 + 40), link)
    assert [b.status_update.seq for b in bodies if member(b) == "status_update"] == [2]


def test_wrong_proof_is_rejected_with_the_vector_frame(vectors: Any) -> None:
    core = _core(vectors)
    link, _ = core.connect(T0)
    auth = decode_handshake(_frame(vectors, "client_auth"), frozenset({"client_auth"}))
    auth.client_auth.client_proof = bytes(32)
    out = core.receive(link, encode_handshake(auth), T0)
    assert out[link].frames == [_frame(vectors, "auth_result_rejected")]
    assert out[link].close_code == CLOSE_AUTH


def test_throttling_after_three_failures(vectors: Any) -> None:
    core = _core(vectors)
    auth = decode_handshake(_frame(vectors, "client_auth"), frozenset({"client_auth"}))
    auth.client_auth.client_proof = bytes(32)
    codes = []
    for attempt in range(4):
        link, _ = core.connect(T0 + attempt)
        codes.append(core.receive(link, encode_handshake(auth), T0 + attempt)[link].close_code)
    assert codes == [CLOSE_AUTH, CLOSE_AUTH, CLOSE_AUTH, CLOSE_THROTTLED]


def test_version_mismatch(vectors: Any) -> None:
    core = _core(vectors)
    link, _ = core.connect(T0)
    auth = decode_handshake(_frame(vectors, "client_auth"), frozenset({"client_auth"}))
    auth.client_auth.proto.major = 2
    assert core.receive(link, encode_handshake(auth), T0)[link].close_code == CLOSE_VERSION


def test_second_client_is_busy_and_same_client_preempts(vectors: Any) -> None:
    core, first, _ = _authenticate(vectors)
    other = decode_handshake(_frame(vectors, "client_auth"), frozenset({"client_auth"}))
    other.client_auth.client_id = bytes(16)
    link, hello = core.connect(T0 + 100)
    server = decode_handshake(hello.frames[0], frozenset({"server_hello"})).server_hello
    inputs = vectors["inputs"]
    other.client_auth.client_proof = crypto.client_proof(
        bytes.fromhex(inputs["k_pair"]),
        server.device_id,
        server.server_nonce,
        other.client_auth.client_nonce,
        other.client_auth.client_id,
    )
    assert core.receive(link, encode_handshake(other), T0 + 100)[link].close_code == CLOSE_BUSY
    again, hello = core.connect(T0 + 200)
    out = core.receive(again, _frame(vectors, "client_auth"), T0 + 200)
    assert out[first].close_code == CLOSE_SESSION
    assert out[again].close_code is None
    assert again in core.links
    assert first not in core.links


def test_replay_and_garbage_close_with_1008(vectors: Any) -> None:
    core, link, app = _authenticate(vectors)
    frame = app.seal(pb.Body(pong=pb.Pong(echo_timestamp_ms=T0)))
    core.receive(link, frame, T0 + 10)
    assert core.receive(link, frame, T0 + 20)[link].close_code == CLOSE_POLICY
    core, link, _ = _authenticate(vectors)
    assert core.receive(link, b"\x01\x02", T0 + 10)[link].close_code == CLOSE_POLICY


def test_rate_limit(vectors: Any) -> None:
    core, link, app = _authenticate(vectors)
    codes = []
    for i in range(params.N_RATE_LIMIT_FRAMES + 1):
        frame = app.seal(pb.Body(status_request=pb.StatusRequest()))
        codes.append(core.receive(link, frame, T0 + i)[link].close_code)
    assert codes[-1] == CLOSE_POLICY
    assert set(codes[:-1]) == {None}


def test_session_and_handshake_timeouts(vectors: Any) -> None:
    core, link, app = _authenticate(vectors)
    app.rx_counter = 2
    out = core.tick(T0 + params.T_SESSION_TO_MS)
    assert out[link].close_code == CLOSE_SESSION
    closing = app.open(out[link].frames[0])
    assert closing.session_close.reason == pb.COMMAND_RESULT_FAILED_TIMEOUT
    pending, _ = core.connect(T0)
    out = core.tick(T0 + params.T_CGW_HANDSHAKE_TO_MS)
    assert out[pending].close_code == CLOSE_THROTTLED
