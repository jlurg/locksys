# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""asyncio APP simulator client (websockets 17): handshake, tagged frames, Ping/Pong.

Connection settings follow LS-SAIC-001 section 8.2: subprotocol ``locksys.v1``, binary frames
only, no WebSocket keep-alive pings, no compression, 256-byte message limit.
"""

import asyncio
import hmac
import secrets
import time
from dataclasses import dataclass, field

from websockets.asyncio.client import ClientConnection, connect
from websockets.typing import Subprotocol

from locksys_hil.errors import ProtocolError
from locksys_hil.gen import locksys_app_pb2 as pb
from locksys_hil.protocols.appsim import crypto
from locksys_hil.protocols.appsim.codec import (
    CGW_TO_APP_MEMBERS,
    FRAME_MAX_BYTES,
    PROTO_MAJOR,
    PROTO_MINOR,
    SUBPROTOCOL,
    IntegrityError,
    SessionChannel,
    decode_handshake,
    encode_handshake,
    member,
)


class AuthenticationError(ProtocolError):
    """The CGW rejected the client or its server proof did not verify."""


@dataclass(frozen=True)
class SessionInfo:
    """Parameters announced in AuthResult(OK)."""

    session_id: int
    session_timeout_ms: int
    keepalive_period_ms: int
    keepalive_timeout_ms: int
    device_id: bytes
    fw_version: str


@dataclass
class AppSimClient:
    """One APP simulator connection.

    Attributes:
        url: WebSocket URL, e.g. ``ws://192.168.4.1:80/ws/v1``.
        k_pair: Pairing key (32 bytes); never logged.
        client_id: Client identifier (16 bytes) of the pairing.
        app_version: Reported in ClientAuth.
    """

    url: str
    k_pair: bytes = field(repr=False)
    client_id: bytes
    app_version: str = "0.1.0"
    channel: SessionChannel | None = field(default=None, init=False, repr=False)
    info: SessionInfo | None = field(default=None, init=False)
    _ws: ClientConnection | None = field(default=None, init=False, repr=False)

    async def connect(self, timeout_s: float = 3.0) -> SessionInfo:
        """Open the connection and authenticate.

        Raises:
            AuthenticationError: rejected authentication or wrong server proof.
            IntegrityError: malformed or unexpected handshake frames.
        """
        self._ws = await asyncio.wait_for(
            connect(
                self.url,
                subprotocols=[Subprotocol(SUBPROTOCOL)],
                compression=None,
                ping_interval=None,
                max_size=FRAME_MAX_BYTES,
            ),
            timeout_s,
        )
        hello = decode_handshake(await self._recv_raw(timeout_s), frozenset({"server_hello"}))
        server = hello.server_hello
        client_nonce = secrets.token_bytes(crypto.NONCE_LENGTH)
        proof = crypto.client_proof(
            self.k_pair, server.device_id, server.server_nonce, client_nonce, self.client_id
        )
        auth = pb.Body(
            client_auth=pb.ClientAuth(
                proto=pb.ProtoVersion(major=PROTO_MAJOR, minor=PROTO_MINOR),
                client_id=self.client_id,
                client_nonce=client_nonce,
                client_proof=proof,
                app_version=self.app_version,
            )
        )
        await self._ws.send(encode_handshake(auth))
        k_sess = crypto.session_key(
            self.k_pair, server.device_id, server.server_nonce, client_nonce, self.client_id
        )
        channel = SessionChannel(k_sess=k_sess, tx_direction=crypto.Direction.APP_TO_CGW)
        raw = await self._recv_raw(timeout_s)
        try:
            body = channel.open(raw)
        except IntegrityError:
            rejected = decode_handshake(raw, frozenset({"auth_result"}))
            result = pb.CommandResult.Name(rejected.auth_result.result)
            raise AuthenticationError(f"authentication rejected: {result}") from None
        if member(body) != "auth_result" or body.auth_result.result != pb.COMMAND_RESULT_OK:
            raise AuthenticationError("AuthResult(OK) expected")
        expected = crypto.server_proof(
            self.k_pair, server.device_id, server.server_nonce, client_nonce, self.client_id
        )
        if not hmac.compare_digest(expected, body.auth_result.server_proof):
            raise AuthenticationError("server proof does not verify")
        self.channel = channel
        self.info = SessionInfo(
            session_id=body.auth_result.session_id,
            session_timeout_ms=body.auth_result.session_timeout_ms,
            keepalive_period_ms=body.auth_result.keepalive_period_ms,
            keepalive_timeout_ms=body.auth_result.keepalive_timeout_ms,
            device_id=server.device_id,
            fw_version=server.fw_version,
        )
        return self.info

    async def _recv_raw(self, timeout_s: float) -> bytes:
        if self._ws is None:
            raise ProtocolError("not connected")
        message = await asyncio.wait_for(self._ws.recv(), timeout_s)
        if isinstance(message, str):
            raise IntegrityError("text frame received")
        return message

    async def send(self, body: pb.Body) -> None:
        """Send one tagged frame."""
        if self._ws is None or self.channel is None:
            raise ProtocolError("no authenticated session")
        await self._ws.send(self.channel.seal(body))

    async def recv(self, timeout_s: float = 1.0, *, answer_ping: bool = True) -> pb.Body:
        """Receive the next frame; Pings are answered with Pongs and skipped when requested."""
        if self.channel is None:
            raise ProtocolError("no authenticated session")
        deadline = time.monotonic() + timeout_s
        while True:
            body = self.channel.open(await self._recv_raw(max(0.0, deadline - time.monotonic())))
            if answer_ping and member(body) == "ping":
                await self.send(pb.Body(pong=pb.Pong(echo_timestamp_ms=body.ping.timestamp_ms)))
                continue
            return body

    async def recv_member(self, name: str, timeout_s: float = 1.0) -> pb.Body:
        """Receive frames until one carries Body member ``name``.

        Raises:
            ValueError: ``name`` is not a CGW -> APP member.
        """
        if name not in CGW_TO_APP_MEMBERS:
            raise ValueError(f"{name} is not sent by the CGW")
        deadline = time.monotonic() + timeout_s
        while True:
            body = await self.recv(max(0.0, deadline - time.monotonic()))
            if member(body) == name:
                return body

    async def close(self) -> None:
        """Close the connection normally (1000)."""
        if self._ws is not None:
            await self._ws.close()
            self._ws = None
        self.channel = None
