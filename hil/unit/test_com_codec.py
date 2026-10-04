# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""CAN codec on the DBC: packing and E2E attributes against the E2E vectors."""

from typing import Any

import pytest

from locksys_hil.protocols.com import ComCodec
from locksys_hil.protocols.e2e import E2eMode


@pytest.fixture(scope="module")
def codec() -> ComCodec:
    return ComCodec()


def test_vector_frames_encode(codec: ComCodec, e2e_vectors: Any) -> None:
    for frame in e2e_vectors["frames"]:
        data = codec.encode_raw(frame["message"], frame["signals_raw"], frame["counter"])
        assert data.hex() == frame["bytes"], frame["name"]


def test_e2e_attributes_match_vectors(codec: ComCodec, e2e_vectors: Any) -> None:
    for name, expected in e2e_vectors["messages"].items():
        spec = codec.e2e_spec(name)
        assert spec is not None, name
        assert spec.mode is E2eMode(expected["mode"])
        assert spec.data_id == expected["data_id"]
        assert spec.max_delta == expected["max_delta"]
        assert spec.rx_timeout_ms == expected["rx_timeout_ms"]
        assert codec.frame_id(name) == expected["frame_id"]


def test_unprotected_messages(codec: ComCodec) -> None:
    assert codec.e2e_spec("CGW_Version") is None
    with pytest.raises(ValueError, match="not E2E protected"):
        codec.receiver("DCU_Version")


def test_decode_round_trip(codec: ComCodec) -> None:
    data = codec.encode_raw("CGW_DoorCmd", {"DoorCmd_Req": 1, "DoorCmd_ReqId": 0x2A}, 0)
    assert data.hex() == "4e102a00"
    decoded = codec.decode(codec.frame_id("CGW_DoorCmd"), data)
    assert decoded["DoorCmd_Req"] == 1
    assert decoded["DoorCmd_ReqId"] == 0x2A
    assert codec.receiver("CGW_DoorCmd").receive(data).delivered


def test_senders_and_cycle_times(codec: ComCodec) -> None:
    assert "CGW_WinCmd" in codec.senders("CGW")
    assert "DCU_WinSts" in codec.senders("DCU")
    assert codec.cycle_time_ms("CGW_WinCmd") == 20
