# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""websockets 17 server binding of the simulator engine.

Endpoint ``ws://<host>:<port>/ws/v1``; the subprotocol ``locksys.v1`` is compared exactly
(mismatch: close 4001); text frames close with 1003; messages above 256 bytes close with 1009;
other paths get HTTP 404. A single asyncio task ticks the engine every ``t_cgw_tick_ms``.
"""

import asyncio
import contextlib
import logging
import time
from collections.abc import Sequence
from http import HTTPStatus

from websockets.asyncio.server import Server, ServerConnection, serve
from websockets.exceptions import ConnectionClosed
from websockets.http11 import Request, Response
from websockets.typing import Subprotocol

from locksys_cgw_sim.core import CLOSE_VERSION, CgwSimCore, Outputs
from locksys_hil.gen import params
from locksys_hil.protocols.appsim.codec import FRAME_MAX_BYTES, SUBPROTOCOL, WS_PATH

_log = logging.getLogger(__name__)
CLOSE_UNSUPPORTED = 1003
CLOSE_TOO_BIG = 1009


class SimServer:
    """Serve one ``CgwSimCore`` on a WebSocket endpoint."""

    def __init__(self, core: CgwSimCore) -> None:
        """Bind the engine; ``start`` opens the socket."""
        self.core = core
        self._connections: dict[int, ServerConnection] = {}
        self._server: Server | None = None
        self._ticker: asyncio.Task[None] | None = None
        self._epoch = time.monotonic()

    def now_ms(self) -> int:
        """Milliseconds since the server object was created."""
        return int((time.monotonic() - self._epoch) * 1000)

    @property
    def port(self) -> int:
        """Bound TCP port (useful with port 0)."""
        if self._server is None:
            raise RuntimeError("server not started")
        return int(next(iter(self._server.sockets)).getsockname()[1])

    async def start(self, host: str = "127.0.0.1", port: int = 8765) -> None:
        """Open the listening socket and start the tick task."""
        self._server = await serve(
            self._handler,
            host,
            port,
            select_subprotocol=self._select_subprotocol,
            process_request=self._process_request,
            compression=None,
            ping_interval=None,
            max_size=FRAME_MAX_BYTES,
        )
        self._ticker = asyncio.create_task(self._tick_loop())

    async def stop(self) -> None:
        """Stop ticking and close all connections."""
        if self._ticker is not None:
            self._ticker.cancel()
            with contextlib.suppress(asyncio.CancelledError):
                await self._ticker
        if self._server is not None:
            self._server.close()
            await self._server.wait_closed()

    @staticmethod
    def _select_subprotocol(
        connection: ServerConnection, offered: Sequence[Subprotocol]
    ) -> Subprotocol | None:
        # Accept the upgrade without a subprotocol on mismatch so the handler closes with 4001.
        return Subprotocol(SUBPROTOCOL) if SUBPROTOCOL in offered else None

    @staticmethod
    def _process_request(connection: ServerConnection, request: Request) -> Response | None:
        if request.path != WS_PATH:
            return connection.respond(HTTPStatus.NOT_FOUND, "not found\n")
        return None

    async def _deliver(self, outputs: Outputs) -> None:
        for link_id, out in outputs.items():
            ws = self._connections.get(link_id)
            if ws is None:
                continue
            try:
                for frame in out.frames:
                    await ws.send(frame)
                if out.close_code is not None:
                    await ws.close(out.close_code)
            except ConnectionClosed:
                self.core.disconnect(link_id)

    async def _tick_loop(self) -> None:
        while True:
            await self._deliver(self.core.tick(self.now_ms()))
            await asyncio.sleep(params.T_CGW_TICK_MS / 1000)

    async def _handler(self, ws: ServerConnection) -> None:
        if ws.subprotocol != SUBPROTOCOL:
            await ws.close(CLOSE_VERSION)
            return
        link_id, hello = self.core.connect(self.now_ms())
        self._connections[link_id] = ws
        try:
            await self._deliver({link_id: hello})
            async for message in ws:
                if isinstance(message, str):
                    await ws.close(CLOSE_UNSUPPORTED)
                    break
                await self._deliver(self.core.receive(link_id, message, self.now_ms()))
        except ConnectionClosed as exc:
            if exc.rcvd is None and exc.sent is not None and exc.sent.code == CLOSE_TOO_BIG:
                _log.info("link %d closed: message too big", link_id)
        finally:
            self._connections.pop(link_id, None)
            self.core.disconnect(link_id)
