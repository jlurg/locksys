# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""E2E reference implementation against ``interfaces/vectors/e2e_v1.json``."""

from typing import Any

import pytest

from locksys_hil.protocols.e2e import (
    E2eMode,
    E2eReceiver,
    E2eSender,
    E2eState,
    E2eStatus,
    crc8_j1850,
    crc_input,
    protect,
)


def test_crc_check_value(e2e_vectors: Any) -> None:
    check = e2e_vectors["crc"]["check"]
    assert crc8_j1850(bytes.fromhex(check["input"])) == check["crc"] == 0x4B


def test_crc_split_input_equals_whole() -> None:
    partial = crc8_j1850(b"1234", final=False)
    assert crc8_j1850(b"56789", partial) == crc8_j1850(b"123456789")


def test_frames(e2e_vectors: Any) -> None:
    assert len(e2e_vectors["frames"]) == 11
    for frame in e2e_vectors["frames"]:
        payload = bytes.fromhex(frame["payload"])
        protected = protect(payload, frame["data_id"], frame["counter"])
        assert protected.hex() == frame["bytes"], frame["name"]
        assert crc_input(protected, frame["data_id"]).hex() == frame["crc_input"], frame["name"]


def _receiver(e2e_vectors: Any, message: str) -> E2eReceiver:
    spec = e2e_vectors["messages"][message]
    profile = e2e_vectors["profile"]
    return E2eReceiver(
        data_id=spec["data_id"],
        dlc=spec["dlc"],
        mode=E2eMode(spec["mode"]),
        max_delta=spec["max_delta"],
        n_ok_valid=profile["n_ok_valid"],
        n_err_invalid=profile["n_err_invalid"],
    )


def test_rx_sequences(e2e_vectors: Any) -> None:
    sequences = e2e_vectors["rx_sequences"]
    assert len(sequences) == 9
    for sequence in sequences:
        receiver = _receiver(e2e_vectors, sequence["message"])
        for index, step in enumerate(sequence["steps"]):
            where = f"{sequence['name']} step {index}"
            if step["event"] == "timeout":
                assert receiver.timeout() == E2eState(step["state"]), where
                continue
            result = receiver.receive(bytes.fromhex(step["bytes"]))
            assert result.status == E2eStatus(step["status"]), where
            assert result.state == E2eState(step["state"]), where
            assert result.delivered is step["delivered"], where
        counters = receiver.counters
        expected = sequence["counters"]
        assert (counters.crc, counters.sequence, counters.repeated, counters.timeout) == (
            expected["crc"],
            expected["sequence"],
            expected["repeated"],
            expected["timeout"],
        ), sequence["name"]


def test_sender_counter_wraps() -> None:
    sender = E2eSender(data_id=0x1001, counter=15)
    first = sender.next_frame(bytes(4))
    second = sender.next_frame(bytes(4))
    assert (first[1] & 0x0F, second[1] & 0x0F) == (15, 0)


def test_protect_keeps_upper_nibble() -> None:
    frame = protect(bytes((0x00, 0x30, 0x2A, 0x00)), 0x1002, 0x0F)
    assert frame[1] == 0x3F


def test_protect_rejects_short_frame() -> None:
    with pytest.raises(ValueError, match="at least 2 bytes"):
        protect(b"\x00", 0x1001, 0)


def test_event_mode_ignores_timeout() -> None:
    receiver = E2eReceiver(data_id=0x1002, dlc=4, mode=E2eMode.EVENT)
    assert receiver.timeout() is E2eState.VALID
    assert receiver.counters.timeout == 0
