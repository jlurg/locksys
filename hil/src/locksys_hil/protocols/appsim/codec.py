# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Frame codec of the APP protocol 1.0 (LS-SAIC-001 sections 8.2 and 8.6).

Receive order: raw payload -> Frame -> tag over the raw body -> counter -> Body -> direction
check. Handshake frames carry counter 0 and an empty tag; session counters start at 1 and
strictly increase per direction.
"""

from dataclasses import dataclass, field
from typing import Final

from google.protobuf.message import DecodeError

from locksys_hil.errors import ProtocolError
from locksys_hil.gen import locksys_app_pb2 as pb
from locksys_hil.protocols.appsim.crypto import Direction, frame_tag, verify_tag

SUBPROTOCOL: Final = "locksys.v1"
WS_PATH: Final = "/ws/v1"
FRAME_MAX_BYTES: Final = 256
PROTO_MAJOR: Final = 1
PROTO_MINOR: Final = 0

APP_TO_CGW_MEMBERS: Final = frozenset(
    {
        "client_auth",
        "ping",
        "pong",
        "door_command",
        "window_move",
        "window_stop",
        "status_request",
    }
)
CGW_TO_APP_MEMBERS: Final = frozenset(
    {
        "server_hello",
        "auth_result",
        "ping",
        "pong",
        "command_ack",
        "door_command_result",
        "status_update",
        "notice",
        "session_close",
    }
)


class IntegrityError(ProtocolError):
    """Bad tag, counter, encoding or direction: close 1008 and STOP latch."""


def member(body: pb.Body) -> str | None:
    """Return the name of the Body member that is set, or None."""
    name = body.WhichOneof("msg")
    return str(name) if name is not None else None


def encode_handshake(body: pb.Body) -> bytes:
    """Serialise a handshake frame (counter 0, empty tag)."""
    return pb.Frame(counter=0, body=body.SerializeToString()).SerializeToString()


def _parse_frame(raw: bytes) -> pb.Frame:
    if len(raw) > FRAME_MAX_BYTES:
        raise IntegrityError(f"frame of {len(raw)} bytes exceeds {FRAME_MAX_BYTES}")
    frame = pb.Frame()
    try:
        frame.ParseFromString(raw)
    except DecodeError as exc:
        raise IntegrityError(f"malformed Frame: {exc}") from exc
    return frame


def _parse_body(data: bytes) -> pb.Body:
    body = pb.Body()
    try:
        body.ParseFromString(data)
    except DecodeError as exc:
        raise IntegrityError(f"malformed Body: {exc}") from exc
    return body


def decode_handshake(raw: bytes, allowed: frozenset[str]) -> pb.Body:
    """Decode a handshake frame and check that its member is one of ``allowed``.

    Raises:
        IntegrityError: malformed frame, non-zero counter, tag present or unexpected member.
    """
    frame = _parse_frame(raw)
    if frame.counter != 0 or frame.tag:
        raise IntegrityError("handshake frame must have counter 0 and no tag")
    body = _parse_body(frame.body)
    name = member(body)
    if name not in allowed:
        raise IntegrityError(f"unexpected handshake member {name}")
    return body


@dataclass
class SessionChannel:
    """Authenticated frame channel of one session endpoint.

    Attributes:
        k_sess: Session key.
        tx_direction: Direction byte of transmitted frames.
        tx_counter: Counter of the last transmitted frame.
        rx_counter: Counter of the last accepted received frame.
    """

    k_sess: bytes = field(repr=False)
    tx_direction: Direction
    tx_counter: int = 0
    rx_counter: int = 0

    @property
    def rx_direction(self) -> Direction:
        """Direction byte expected on received frames."""
        return (
            Direction.CGW_TO_APP
            if self.tx_direction is Direction.APP_TO_CGW
            else Direction.APP_TO_CGW
        )

    @property
    def rx_members(self) -> frozenset[str]:
        """Body members valid in the receive direction."""
        return (
            CGW_TO_APP_MEMBERS if self.rx_direction is Direction.CGW_TO_APP else APP_TO_CGW_MEMBERS
        )

    def seal(self, body: pb.Body) -> bytes:
        """Serialise ``body`` as the next tagged frame."""
        self.tx_counter += 1
        data = body.SerializeToString()
        tag = frame_tag(self.k_sess, self.tx_direction, self.tx_counter, data)
        return pb.Frame(counter=self.tx_counter, body=data, tag=tag).SerializeToString()

    def open(self, raw: bytes) -> pb.Body:
        """Verify and decode a received frame.

        Raises:
            IntegrityError: any check of LS-SAIC-001 section 8.2 fails.
        """
        frame = _parse_frame(raw)
        if not verify_tag(self.k_sess, self.rx_direction, frame.counter, frame.body, frame.tag):
            raise IntegrityError("bad tag")
        if frame.counter == 0 or frame.counter <= self.rx_counter:
            raise IntegrityError(f"counter {frame.counter} not above {self.rx_counter}")
        body = _parse_body(frame.body)
        name = member(body)
        if name is not None and name not in self.rx_members:
            raise IntegrityError(f"{name} not allowed in this direction")
        self.rx_counter = frame.counter
        return body
