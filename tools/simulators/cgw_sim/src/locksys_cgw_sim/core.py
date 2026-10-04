# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Sans-I/O session engine of the CGW simulator (LS-SAIC-001 sections 8.2 to 8.10).

Every entry point takes the current time in milliseconds and returns, per link, the frames to
send and an optional WebSocket close code, so the engine is deterministic and testable without
sockets. Implemented: handshake with mutual authentication, failure throttling, single
controller with pre-emption and REJECTED_BUSY, tagged frames with counter checks, rate limit,
session timeout, Ping/Pong with RTT gate, window press admission with keep-alive supervision and
CGW backstop, door transactions with re-acknowledgement and result cache, status push.
"""

import hmac
import secrets
from collections import deque
from collections.abc import Callable
from dataclasses import dataclass, field
from enum import Enum
from typing import Final

from locksys_cgw_sim.plant import DoorResult, Plant
from locksys_hil.gen import dtc, params
from locksys_hil.gen import locksys_app_pb2 as pb
from locksys_hil.protocols.appsim import crypto
from locksys_hil.protocols.appsim.codec import (
    PROTO_MAJOR,
    IntegrityError,
    SessionChannel,
    decode_handshake,
    encode_handshake,
    member,
)

CLOSE_NORMAL: Final = 1000
CLOSE_POLICY: Final = 1008
CLOSE_VERSION: Final = 4001
CLOSE_AUTH: Final = 4002
CLOSE_BUSY: Final = 4003
CLOSE_SESSION: Final = 4004
CLOSE_THROTTLED: Final = 4006

_NOTICE_TEXT: Final[dict[int, str]] = {
    dtc.NOTICE_WINDOW_STOP_KA_TIMEOUT: "Keep-alive timeout",
    dtc.NOTICE_WINDOW_STOP_LINK_QUALITY: "Link quality",
    dtc.NOTICE_WINDOW_STOP_DIR_CHANGE: "Direction change within a press",
    dtc.NOTICE_WINDOW_STOP_COMM: "DCU communication lost",
    dtc.NOTICE_WINDOW_STOP_MODE: "Mode change",
    dtc.NOTICE_WINDOW_STOP_BACKSTOP: "CGW run-time backstop",
    dtc.NOTICE_WINDOW_STOP_DCU: "Stopped by the DCU",
}
_SEVERITY: Final[dict[str, pb.FaultSeverity]] = {
    "INFO": pb.FAULT_SEVERITY_INFO,
    "WARNING": pb.FAULT_SEVERITY_WARNING,
    "DEGRADED": pb.FAULT_SEVERITY_DEGRADED,
    "CRITICAL": pb.FAULT_SEVERITY_CRITICAL,
}
_NOTICE_SEVERITY: Final[dict[int, pb.FaultSeverity]] = {
    n.code: _SEVERITY[n.severity] for n in dtc.NOTICES
}


@dataclass(frozen=True)
class SimConfig:
    """Identity and secrets of the simulated CGW.

    Attributes:
        k_pair: Pairing key (32 bytes).
        device_id: 8-byte device identifier.
        fw_version: Reported in ServerHello.
        random_bytes: Source of nonces and session identifiers.
    """

    k_pair: bytes = field(repr=False)
    device_id: bytes
    fw_version: str = "0.1.0-dev+sim"
    random_bytes: Callable[[int], bytes] = secrets.token_bytes


@dataclass
class Output:
    """Reaction for one link: frames to send, then an optional close."""

    frames: list[bytes] = field(default_factory=list)
    close_code: int | None = None


Outputs = dict[int, Output]


class LinkState(Enum):
    """Life cycle of one WebSocket connection."""

    HANDSHAKE = "handshake"
    SESSION = "session"
    CLOSED = "closed"


@dataclass
class _Press:
    press_id: int
    direction: pb.WindowDirection
    started_ms: int
    last_move_ms: int


@dataclass
class _Link:
    link_id: int
    opened_ms: int
    server_nonce: bytes
    state: LinkState = LinkState.HANDSHAKE
    channel: SessionChannel | None = None
    client_id: bytes = b""
    last_rx_ms: int = 0
    rx_times: deque[int] = field(default_factory=deque)
    last_press_id: int = 0
    press: _Press | None = None
    latched_press_id: int | None = None
    door_inflight: int | None = None
    door_cache: dict[int, tuple[pb.Body, int]] = field(default_factory=dict)
    last_door_request_id: int = 0
    last_door_result: pb.CommandResult = pb.COMMAND_RESULT_UNSPECIFIED
    last_ping_ms: int = 0
    pending_ping: tuple[int, int] | None = None
    rtt_ms: int | None = None
    rtt_sample_ms: int = 0
    status_seq: int = 0
    status_due: bool = True
    last_status_ms: int = 0
    last_status_key: tuple[int, ...] | None = None


class CgwSimCore:
    """Session engine; one instance serves all connections of one simulated CGW."""

    def __init__(self, config: SimConfig, plant: Plant | None = None) -> None:
        """Create the engine with an optional plant model."""
        self.config = config
        self.plant = plant or Plant()
        self.links: dict[int, _Link] = {}
        self._next_link = 1
        self._auth_failures = 0
        self._last_failure_ms: int | None = None

    # -- connection life cycle ------------------------------------------------------------

    def connect(self, now_ms: int) -> tuple[int, Output]:
        """Register a new WebSocket connection and return its ServerHello."""
        link = _Link(
            link_id=self._next_link,
            opened_ms=now_ms,
            server_nonce=self.config.random_bytes(crypto.NONCE_LENGTH),
        )
        self._next_link += 1
        self.links[link.link_id] = link
        hello = pb.Body(
            server_hello=pb.ServerHello(
                proto=pb.ProtoVersion(major=PROTO_MAJOR, minor=0),
                device_id=self.config.device_id,
                server_nonce=link.server_nonce,
                fw_version=self.config.fw_version,
                com_matrix=pb.ProtoVersion(major=1, minor=0),
                pairing_window_open=False,
            )
        )
        return link.link_id, Output([encode_handshake(hello)])

    def disconnect(self, link_id: int) -> None:
        """Forget a connection closed by the peer; an active press is latched (STOP)."""
        link = self.links.pop(link_id, None)
        if link is not None and link.press is not None:
            self.plant.stop_window(pb.WINDOW_STOP_REASON_RELEASED)

    def _close(self, link: _Link, out: Output, code: int) -> Output:
        if link.press is not None:
            self.plant.stop_window(pb.WINDOW_STOP_REASON_RELEASED)
            link.press = None
        link.state = LinkState.CLOSED
        out.close_code = code
        self.links.pop(link.link_id, None)
        return out

    # -- receive ----------------------------------------------------------------------------

    def receive(self, link_id: int, raw: bytes, now_ms: int) -> Outputs:
        """Process one received WebSocket binary message."""
        link = self.links.get(link_id)
        if link is None:
            return {}
        if link.state is LinkState.HANDSHAKE:
            return self._handshake(link, raw, now_ms)
        return {link_id: self._session_frame(link, raw, now_ms)}

    def _throttled(self, now_ms: int) -> bool:
        if (
            self._last_failure_ms is not None
            and now_ms - self._last_failure_ms >= params.T_AUTH_FAIL_DECAY_MS
        ):
            self._auth_failures = 0
        return (
            self._auth_failures >= params.N_AUTH_FAIL_THROTTLE
            and self._last_failure_ms is not None
            and now_ms - self._last_failure_ms < params.T_AUTH_THROTTLE_MS
        )

    def _fail(self, now_ms: int) -> None:
        self._auth_failures += 1
        self._last_failure_ms = now_ms

    def _reject(self, link: _Link, result: pb.CommandResult, code: int) -> Outputs:
        body = pb.Body(auth_result=pb.AuthResult(result=result, server_proof=bytes(32)))
        return {link.link_id: self._close(link, Output([encode_handshake(body)]), code)}

    def _controller(self) -> _Link | None:
        for link in self.links.values():
            if link.state is LinkState.SESSION:
                return link
        return None

    def _handshake(self, link: _Link, raw: bytes, now_ms: int) -> Outputs:
        try:
            auth = decode_handshake(raw, frozenset({"client_auth"})).client_auth
        except IntegrityError:
            self._fail(now_ms)
            return {link.link_id: self._close(link, Output(), CLOSE_POLICY)}
        if self._throttled(now_ms):
            return self._reject(link, pb.COMMAND_RESULT_REJECTED_RATE_LIMIT, CLOSE_THROTTLED)
        if auth.proto.major != PROTO_MAJOR:
            return self._reject(link, pb.COMMAND_RESULT_REJECTED_VERSION, CLOSE_VERSION)
        cfg = self.config
        expected = crypto.client_proof(
            cfg.k_pair, cfg.device_id, link.server_nonce, auth.client_nonce, auth.client_id
        )
        if (
            len(auth.client_id) != crypto.CLIENT_ID_LENGTH
            or len(auth.client_nonce) != crypto.NONCE_LENGTH
            or not hmac.compare_digest(expected, auth.client_proof)
        ):
            self._fail(now_ms)
            return self._reject(link, pb.COMMAND_RESULT_REJECTED_AUTH, CLOSE_AUTH)
        outputs: Outputs = {}
        controller = self._controller()
        if controller is not None:
            alive = now_ms - controller.last_rx_ms < params.T_SESSION_TO_MS
            if alive and controller.client_id != auth.client_id:
                return self._reject(link, pb.COMMAND_RESULT_REJECTED_BUSY, CLOSE_BUSY)
            # Pre-emption by the same client, or replacement of a silent controller.
            reason = pb.COMMAND_RESULT_REJECTED_BUSY if alive else pb.COMMAND_RESULT_FAILED_TIMEOUT
            outputs[controller.link_id] = self._end_session(controller, reason)
        k_sess = crypto.session_key(
            cfg.k_pair, cfg.device_id, link.server_nonce, auth.client_nonce, auth.client_id
        )
        link.channel = SessionChannel(k_sess=k_sess, tx_direction=crypto.Direction.CGW_TO_APP)
        link.state = LinkState.SESSION
        link.client_id = bytes(auth.client_id)
        link.last_rx_ms = now_ms
        result = pb.Body(
            auth_result=pb.AuthResult(
                result=pb.COMMAND_RESULT_OK,
                server_proof=crypto.server_proof(
                    cfg.k_pair, cfg.device_id, link.server_nonce, auth.client_nonce, auth.client_id
                ),
                session_id=int.from_bytes(cfg.random_bytes(4), "big"),
                session_timeout_ms=params.T_SESSION_TO_MS,
                keepalive_period_ms=params.T_APP_KA_MS,
                keepalive_timeout_ms=params.T_CGW_KA_TO_MS,
            )
        )
        out = Output([link.channel.seal(result), self._ping(link, now_ms)])
        outputs[link.link_id] = out
        return outputs

    def _end_session(self, link: _Link, reason: pb.CommandResult) -> Output:
        frames = []
        if link.channel is not None:
            frames.append(link.channel.seal(pb.Body(session_close=pb.SessionClose(reason=reason))))
        return self._close(link, Output(frames), CLOSE_SESSION)

    def _session_frame(self, link: _Link, raw: bytes, now_ms: int) -> Output:
        assert link.channel is not None
        link.rx_times.append(now_ms)
        while link.rx_times and link.rx_times[0] <= now_ms - params.T_RATE_WINDOW_MS:
            link.rx_times.popleft()
        if len(link.rx_times) > params.N_RATE_LIMIT_FRAMES:
            return self._close(link, Output(), CLOSE_POLICY)
        try:
            body = link.channel.open(raw)
        except IntegrityError:
            return self._close(link, Output(), CLOSE_POLICY)
        link.last_rx_ms = now_ms
        replies = self._dispatch(link, body, now_ms)
        return Output([link.channel.seal(reply) for reply in replies])

    def _dispatch(self, link: _Link, body: pb.Body, now_ms: int) -> list[pb.Body]:
        name = member(body)
        if name == "ping":
            return [pb.Body(pong=pb.Pong(echo_timestamp_ms=body.ping.timestamp_ms))]
        if name == "pong":
            if link.pending_ping and body.pong.echo_timestamp_ms == link.pending_ping[0]:
                link.rtt_ms = now_ms - link.pending_ping[1]
                link.rtt_sample_ms = now_ms
                link.pending_ping = None
            return []
        if name == "window_move":
            return self._window_move(link, body.window_move, now_ms)
        if name == "window_stop":
            return self._window_stop(link, body.window_stop.press_id)
        if name == "door_command":
            return self._door(link, body.door_command, now_ms)
        if name == "status_request":
            link.status_due = True
        return []

    # -- window -----------------------------------------------------------------------------

    @staticmethod
    def _ack(kind: pb.CommandKind, ref_id: int, result: pb.CommandResult) -> pb.Body:
        return pb.Body(command_ack=pb.CommandAck(kind=kind, ref_id=ref_id, result=result))

    def _admit(self, link: _Link, move: pb.WindowMove, now_ms: int) -> pb.CommandResult:
        plant = self.plant
        if not plant.dcu_alive:
            return pb.COMMAND_RESULT_FAILED_COMM
        if plant.dcu_mode not in (pb.NODE_MODE_NORMAL, pb.NODE_MODE_DEGRADED):
            return pb.COMMAND_RESULT_REJECTED_MODE
        rtt_fresh = now_ms - link.rtt_sample_ms <= params.T_RTT_SAMPLE_MAX_AGE_MS
        if link.rtt_ms is None or link.rtt_ms > params.T_RTT_MAX_MS or not rtt_fresh:
            return pb.COMMAND_RESULT_REJECTED_LINK_QUALITY
        if (
            move.press_id <= link.last_press_id
            or move.direction not in (pb.WINDOW_DIRECTION_UP, pb.WINDOW_DIRECTION_DOWN)
            or move.hold_ms > params.T_NEW_PRESS_MAX_MS
        ):
            return pb.COMMAND_RESULT_REJECTED_INVALID
        return pb.COMMAND_RESULT_ACCEPTED

    def _window_move(self, link: _Link, move: pb.WindowMove, now_ms: int) -> list[pb.Body]:
        press = link.press
        if press is not None and move.press_id == press.press_id:
            if move.direction != press.direction:
                return self._latch(
                    link, pb.COMMAND_RESULT_REJECTED_INVALID, dtc.NOTICE_WINDOW_STOP_DIR_CHANGE
                )
            press.last_move_ms = now_ms
            return []
        if move.press_id == link.latched_press_id:
            return []
        result = self._admit(link, move, now_ms)
        link.last_press_id = max(link.last_press_id, move.press_id)
        if result != pb.COMMAND_RESULT_ACCEPTED:
            link.latched_press_id = move.press_id
            return [self._ack(pb.COMMAND_KIND_WINDOW, move.press_id, result)]
        if press is not None:
            self.plant.stop_window(pb.WINDOW_STOP_REASON_RELEASED)
        link.press = _Press(move.press_id, move.direction, now_ms, now_ms)
        link.latched_press_id = None
        self.plant.start_window(move.direction)
        return [self._ack(pb.COMMAND_KIND_WINDOW, move.press_id, result)]

    def _window_stop(self, link: _Link, press_id: int) -> list[pb.Body]:
        if link.press is not None and link.press.press_id == press_id:
            self.plant.stop_window(pb.WINDOW_STOP_REASON_RELEASED)
            link.press = None
        elif link.latched_press_id == press_id:
            link.latched_press_id = None
        return []

    def _latch(self, link: _Link, result: pb.CommandResult, notice: int) -> list[pb.Body]:
        press = link.press
        if press is None:
            return []
        self.plant.stop_window(pb.WINDOW_STOP_REASON_RELEASED)
        link.press = None
        link.latched_press_id = press.press_id
        return [
            self._ack(pb.COMMAND_KIND_WINDOW, press.press_id, result),
            pb.Body(
                notice=pb.Notice(
                    severity=_NOTICE_SEVERITY[notice], code=notice, text=_NOTICE_TEXT[notice]
                )
            ),
        ]

    # -- door -------------------------------------------------------------------------------

    def _door_result(self, link: _Link, request_id: int, done: DoorResult, now_ms: int) -> pb.Body:
        body = pb.Body(
            door_command_result=pb.DoorCommandResult(
                request_id=request_id, result=done.result, lock_state=done.lock_state
            )
        )
        link.door_cache[request_id] = (body, now_ms)
        link.last_door_result = done.result
        return body

    def _door(self, link: _Link, cmd: pb.DoorCommand, now_ms: int) -> list[pb.Body]:
        rid = cmd.request_id
        if link.door_inflight == rid:
            return [self._ack(pb.COMMAND_KIND_DOOR, rid, pb.COMMAND_RESULT_ACCEPTED)]
        cached = link.door_cache.get(rid)
        if cached is not None and now_ms - cached[1] <= params.T_CGW_DOOR_CACHE_MS:
            return [cached[0]]
        plant = self.plant
        result: pb.CommandResult
        if cmd.action not in (pb.DOOR_ACTION_LOCK, pb.DOOR_ACTION_UNLOCK):
            result = pb.COMMAND_RESULT_REJECTED_INVALID
        elif not plant.dcu_alive:
            result = pb.COMMAND_RESULT_FAILED_COMM
        elif plant.dcu_mode not in (pb.NODE_MODE_NORMAL, pb.NODE_MODE_DEGRADED):
            result = pb.COMMAND_RESULT_REJECTED_MODE
        elif link.door_inflight is not None or plant.door_busy:
            result = pb.COMMAND_RESULT_REJECTED_BUSY
        else:
            result = pb.COMMAND_RESULT_ACCEPTED
        replies = [self._ack(pb.COMMAND_KIND_DOOR, rid, result)]
        if result != pb.COMMAND_RESULT_ACCEPTED:
            return replies
        link.last_door_request_id = rid
        immediate = plant.request_door(cmd.action, now_ms)
        if immediate is not None:
            replies.append(self._door_result(link, rid, immediate, now_ms))
        else:
            link.door_inflight = rid
        return replies

    # -- periodic ---------------------------------------------------------------------------

    def _ping(self, link: _Link, now_ms: int) -> bytes:
        assert link.channel is not None
        stamp = now_ms & 0xFFFFFFFF
        link.pending_ping = (stamp, now_ms)
        link.last_ping_ms = now_ms
        return link.channel.seal(pb.Body(ping=pb.Ping(timestamp_ms=stamp)))

    def _status(self, link: _Link, now_ms: int) -> pb.Body:
        p = self.plant
        alive = p.dcu_alive
        link.status_seq += 1
        link.last_status_ms = now_ms
        return pb.Body(
            status_update=pb.StatusUpdate(
                seq=link.status_seq,
                door_lock_state=p.lock_state if alive else pb.DOOR_LOCK_STATE_UNKNOWN,
                window_state=p.window_state if alive else pb.WINDOW_STATE_UNKNOWN,
                window_position_pct=255,
                window_stop_reason=p.stop_reason if alive else pb.WINDOW_STOP_REASON_NONE,
                temperature_cdeg=p.temperature_cdeg if alive else 0,
                temp_status=p.temp_status if alive else pb.TEMP_STATUS_STALE,
                dcu_mode=p.dcu_mode if alive else pb.NODE_MODE_UNKNOWN,
                cgw_mode=pb.NODE_MODE_NORMAL,
                dcu_alive=alive,
                vbat_dv=p.vbat_dv if alive else 0,
                last_door_request_id=link.last_door_request_id,
                last_door_result=link.last_door_result,
                window_speed_rpm_x10=p.speed_rpm_x10 if alive else 0,
                window_encoder_status=(
                    pb.ENCODER_STATUS_OK if alive else pb.ENCODER_STATUS_UNKNOWN
                ),
                window_inhibited=not alive,
                door_inhibited=not alive,
            )
        )

    def _status_key(self) -> tuple[int, ...]:
        p = self.plant
        return (
            p.lock_state,
            p.window_state,
            p.stop_reason,
            int(p.dcu_alive),
            p.dcu_mode,
            p.temperature_cdeg // params.TEMP_PUSH_DELTA_CDEG,
        )

    def tick(self, now_ms: int) -> Outputs:
        """Advance timers: plant, supervision, pings, door results and status pushes."""
        outputs: Outputs = {}
        door_done = self.plant.tick(now_ms)
        for link in list(self.links.values()):
            if link.state is LinkState.HANDSHAKE:
                if now_ms - link.opened_ms >= params.T_CGW_HANDSHAKE_TO_MS:
                    self._fail(now_ms)
                    outputs[link.link_id] = self._close(link, Output(), CLOSE_THROTTLED)
                continue
            out = self._tick_session(link, now_ms, door_done)
            if out.frames or out.close_code is not None:
                outputs[link.link_id] = out
        return outputs

    def _tick_session(self, link: _Link, now_ms: int, door_done: DoorResult | None) -> Output:
        assert link.channel is not None
        if now_ms - link.last_rx_ms >= params.T_SESSION_TO_MS:
            return self._end_session(link, pb.COMMAND_RESULT_FAILED_TIMEOUT)
        replies: list[pb.Body] = []
        press = link.press
        if press is not None:
            ping_late = (
                link.pending_ping is not None
                and now_ms - link.pending_ping[1] > params.T_PONG_TO_MS
            )
            if now_ms - press.last_move_ms > params.T_CGW_KA_TO_MS:
                replies += self._latch(
                    link, pb.COMMAND_RESULT_FAILED_TIMEOUT, dtc.NOTICE_WINDOW_STOP_KA_TIMEOUT
                )
            elif now_ms - press.started_ms >= params.T_CGW_MAX_RUN_BACKSTOP_MS:
                replies += self._latch(
                    link, pb.COMMAND_RESULT_FAILED_TIMEOUT, dtc.NOTICE_WINDOW_STOP_BACKSTOP
                )
            elif ping_late or (link.rtt_ms is not None and link.rtt_ms > params.T_RTT_MAX_MS):
                replies += self._latch(
                    link,
                    pb.COMMAND_RESULT_REJECTED_LINK_QUALITY,
                    dtc.NOTICE_WINDOW_STOP_LINK_QUALITY,
                )
        if door_done is not None and link.door_inflight is not None:
            replies.append(self._door_result(link, link.door_inflight, door_done, now_ms))
            link.door_inflight = None
        key = self._status_key()
        period = (
            params.T_STATUS_PUSH_MOTION_MS
            if link.press is not None or self.plant.moving
            else params.T_STATUS_PUSH_IDLE_MS
        )
        if link.status_due or key != link.last_status_key or now_ms - link.last_status_ms >= period:
            link.status_due = False
            link.last_status_key = key
            replies.append(self._status(link, now_ms))
        frames = [link.channel.seal(reply) for reply in replies]
        ping_period = params.T_PING_MOTION_MS if link.press is not None else params.T_PING_IDLE_MS
        if link.pending_ping is not None and now_ms - link.pending_ping[1] > params.T_PONG_TO_MS:
            link.pending_ping = None
        if link.pending_ping is None and now_ms - link.last_ping_ms >= ping_period:
            frames.append(self._ping(link, now_ms))
        return Output(frames)
