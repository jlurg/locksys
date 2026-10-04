# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""End-to-end test of the WebSocket server with the HIL APP simulator client."""

import asyncio
import secrets

import pytest
from websockets.asyncio.client import connect
from websockets.exceptions import ConnectionClosed, InvalidStatus
from websockets.typing import Subprotocol

from locksys_cgw_sim.__main__ import pairing_payload
from locksys_cgw_sim.core import CLOSE_VERSION, CgwSimCore, SimConfig
from locksys_cgw_sim.server import SimServer
from locksys_hil.gen import locksys_app_pb2 as pb
from locksys_hil.protocols.appsim.client import AppSimClient, AuthenticationError


async def _scenario() -> None:
    config = SimConfig(k_pair=secrets.token_bytes(32), device_id=bytes(range(8)))
    server = SimServer(CgwSimCore(config))
    await server.start("127.0.0.1", 0)
    url = f"ws://127.0.0.1:{server.port}/ws/v1"
    try:
        client = AppSimClient(url, k_pair=config.k_pair, client_id=bytes(16))
        info = await client.connect()
        assert info.keepalive_timeout_ms == 350
        assert info.device_id == config.device_id
        status = await client.recv_member("status_update", timeout_s=2.0)
        assert status.status_update.dcu_alive
        await asyncio.sleep(0.05)  # the Ping was answered by recv(); RTT sample exists
        move = pb.WindowMove(press_id=1, direction=pb.WINDOW_DIRECTION_DOWN, hold_ms=0)
        await client.send(pb.Body(window_move=move))
        ack = await client.recv_member("command_ack", timeout_s=2.0)
        assert ack.command_ack.result == pb.COMMAND_RESULT_ACCEPTED
        await client.send(pb.Body(window_stop=pb.WindowStop(press_id=1)))
        await client.close()

        intruder = AppSimClient(url, k_pair=bytes(32), client_id=bytes(16))
        with pytest.raises(AuthenticationError, match="REJECTED_AUTH"):
            await intruder.connect()
        await intruder.close()

        async with connect(url, subprotocols=[Subprotocol("locksys.v2")]) as ws:
            with pytest.raises(ConnectionClosed) as closed:
                await ws.recv()
            assert closed.value.rcvd is not None
            assert closed.value.rcvd.code == CLOSE_VERSION

        with pytest.raises(InvalidStatus):
            async with connect(f"ws://127.0.0.1:{server.port}/other"):
                pass
    finally:
        await server.stop()


def test_server_end_to_end() -> None:
    asyncio.run(asyncio.wait_for(_scenario(), 20))


def test_pairing_payload() -> None:
    config = SimConfig(k_pair=bytes(32), device_id=bytes.fromhex("0102030405060708"))
    payload = pairing_payload(config, "127.0.0.1", 8765)
    assert payload.startswith("locksys://pair?v=1&id=0102030405060708&")
    assert "&k=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA&" in payload
    assert payload.endswith("&h=127.0.0.1:8765")
