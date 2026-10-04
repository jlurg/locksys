# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Build and verify the shared test vectors in interfaces/vectors/.

The vectors are computed with the independent reference in refimpl.py and the cantools
packing of interfaces/can/locksys.dbc, and compared with the values printed in LS-SAIC-001
sections 7.10 and 8.6.

Usage:
    uv run tools/codegen/vectors.py           # verify the committed JSON files (default)
    uv run tools/codegen/vectors.py --write   # rewrite the JSON files from the reference
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from tools.codegen import refimpl, saic
from tools.codegen.common import REPO_ROOT, CodegenError, rel, write_text

E2E_JSON = Path("interfaces/vectors/e2e_v1.json")
SESSION_JSON = Path("interfaces/vectors/app_session_v1.json")
KAT_JSON = Path("interfaces/vectors/crypto_kat.json")
DBC = Path("interfaces/can/locksys.dbc")

# --------------------------------------------------------------------------- E2E frames

#: (name, message, contract content text, alive counter, raw signal values) in the order of
#: the LS-SAIC-001 section 7.10 table (after the CRC check value row).
E2E_FRAMES: list[tuple[str, str, str, int, dict[str, int]]] = [
    (
        "wincmd_up",
        "CGW_WinCmd",
        "UP, counter 3, PressId 7, HoldAge 50 ms",
        3,
        {"WinCmd_Req": 1, "WinCmd_PressId": 7, "WinCmd_HoldAge": 5},
    ),
    (
        "wincmd_stop",
        "CGW_WinCmd",
        "STOP, counter 4, PressId 7, HoldAge 0",
        4,
        {"WinCmd_Req": 0, "WinCmd_PressId": 7, "WinCmd_HoldAge": 0},
    ),
    (
        "wincmd_startup",
        "CGW_WinCmd",
        "CGW start-up: STOP, counter 0, PressId 0, HoldAge raw 255",
        0,
        {"WinCmd_Req": 0, "WinCmd_PressId": 0, "WinCmd_HoldAge": 255},
    ),
    (
        "doorcmd_lock",
        "CGW_DoorCmd",
        "LOCK, counter 0, ReqId 0x2A",
        0,
        {"DoorCmd_Req": 1, "DoorCmd_ReqId": 0x2A},
    ),
    (
        "cgw_nodests_normal",
        "CGW_NodeSts",
        "counter 0, NORMAL, CAN matrix 1.0, AUTHENTICATED, 1 client, POWER_ON, 0 DTCs, heap 0 %",
        0,
        {
            "CgwSts_Mode": 2,
            "CgwSts_ComVerMajor": 1,
            "CgwSts_ComVerMinor": 0,
            "CgwSts_AppLink": 2,
            "CgwSts_WifiClients": 1,
            "CgwSts_ResetReason": 1,
        },
    ),
    (
        "winsts_moving_up",
        "DCU_WinSts",
        "counter 5, MOVING_UP, PosPct 255, StopReason NONE, 0.4 A, PressIdEcho 7, WinResult ACCEPTED",
        5,
        {
            "WinSts_State": 2,
            "WinSts_PosPct": 255,
            "WinSts_StopReason": 0,
            "WinSts_Current": 4,
            "WinSts_PressIdEcho": 7,
            "WinSts_WinResult": 2,
        },
    ),
    (
        "winsts_blocked",
        "DCU_WinSts",
        "counter 6, BLOCKED, PosPct 255, StopReason STALL, 0.0 A, PressIdEcho 7, FltStall 1, "
        "WinResult FAILED_ACTUATOR",
        6,
        {
            "WinSts_State": 6,
            "WinSts_PosPct": 255,
            "WinSts_StopReason": 7,
            "WinSts_Current": 0,
            "WinSts_PressIdEcho": 7,
            "WinSts_FltStall": 1,
            "WinSts_WinResult": 8,
        },
    ),
    (
        "winmotion_up",
        "DCU_WinMotion",
        "counter 2, EncoderStatus OK, +170.0 rpm, duty 100 %, PosCounts \u22122640",
        2,
        {
            "WinMot_EncSts": 1,
            "WinMot_Speed": 1700,
            "WinMot_DutyPct": 100,
            "WinMot_PosCounts": -2640,
        },
    ),
    (
        "doorsts_locking",
        "DCU_DoorSts",
        "counter 0, LOCKING, LastReqId 0x2A, LastResult ACCEPTED",
        0,
        {"DoorSts_LockState": 3, "DoorSts_LastReqId": 0x2A, "DoorSts_LastResult": 2},
    ),
    (
        "tempsts_valid",
        "DCU_TempSts",
        "counter 1, VALID, 23.45 °C, SampleSeq 42",
        1,
        {"TempSts_Status": 1, "TempSts_Value": 2345, "TempSts_SampleSeq": 42},
    ),
    (
        "dcu_nodests_normal",
        "DCU_NodeSts",
        "counter 0, NORMAL, CAN matrix 1.0, 12.0 V, POWER_ON, 0 DTCs, CPU 7 %, no inhibits",
        0,
        {
            "DcuSts_Mode": 2,
            "DcuSts_ComVerMajor": 1,
            "DcuSts_Vbat": 120,
            "DcuSts_ResetReason": 1,
            "DcuSts_CpuLoadMax": 7,
        },
    ),
]

#: Receiver scenarios: (name, message, description, steps). A step is ``("frame", counter,
#: fault)`` with fault "" (none), "crc" (one payload bit flipped after protection) or "dlc"
#: (last byte dropped), or ``("timeout",)``.
RX_SCENARIOS: list[tuple[str, str, str, list[tuple[Any, ...]]]] = [
    (
        "startup_to_valid",
        "CGW_WinCmd",
        "The first frame sets the reference; two consecutive OK frames give VALID.",
        [("frame", 0, ""), ("frame", 1, ""), ("frame", 2, "")],
    ),
    (
        "loss_within_max_delta",
        "CGW_WinCmd",
        "One lost frame (delta 2) is OK for CGW_WinCmd (MaxDelta 2); delta 3 resynchronises.",
        [
            ("frame", 0, ""),
            ("frame", 1, ""),
            ("frame", 3, ""),
            ("frame", 6, ""),
            ("frame", 7, ""),
            ("frame", 8, ""),
        ],
    ),
    (
        "repeated_frame",
        "CGW_WinCmd",
        "A repeated counter is discarded and leaves the reference unchanged.",
        [("frame", 4, ""), ("frame", 5, ""), ("frame", 5, ""), ("frame", 6, "")],
    ),
    (
        "crc_errors_to_invalid",
        "CGW_WinCmd",
        "Three consecutive CRC or DLC errors give INVALID; the next frame resynchronises and two "
        "consecutive OK frames give VALID again.",
        [
            ("frame", 0, ""),
            ("frame", 1, ""),
            ("frame", 2, "crc"),
            ("frame", 3, "dlc"),
            ("frame", 4, "crc"),
            ("frame", 5, ""),
            ("frame", 6, ""),
            ("frame", 7, ""),
        ],
    ),
    (
        "error_while_valid",
        "CGW_WinCmd",
        "Fewer than three errors keep VALID; the next OK frame is delivered.",
        [
            ("frame", 0, ""),
            ("frame", 1, ""),
            ("frame", 2, "crc"),
            ("frame", 2, ""),
            ("frame", 3, ""),
        ],
    ),
    (
        "timeout_clears_reference",
        "CGW_WinCmd",
        "An RX timeout gives INVALID and clears the reference; any counter is then accepted.",
        [("frame", 0, ""), ("frame", 1, ""), ("timeout",), ("frame", 9, ""), ("frame", 10, "")],
    ),
    (
        "counter_wrap",
        "CGW_WinCmd",
        "The alive counter wraps from 15 to 0.",
        [("frame", 14, ""), ("frame", 15, ""), ("frame", 0, ""), ("frame", 1, "")],
    ),
    (
        "max_delta_3",
        "DCU_TempSts",
        "MaxDelta 3: delta 3 is OK, delta 4 is a sequence error.",
        [
            ("frame", 0, ""),
            ("frame", 3, ""),
            ("frame", 6, ""),
            ("frame", 10, ""),
            ("frame", 11, ""),
        ],
    ),
    (
        "event_mode",
        "CGW_DoorCmd",
        "Event mode: CRC and DLC are checked, the counter is not sequence-checked.",
        [
            ("frame", 0, ""),
            ("frame", 0, ""),
            ("frame", 7, ""),
            ("frame", 8, "crc"),
            ("frame", 9, ""),
        ],
    ),
]


def _load_dbc(repo_root: Path) -> Any:
    import cantools

    return cantools.database.load_file(str(repo_root / DBC), strict=True)


def _message_config(message: Any) -> dict[str, Any]:
    attributes = message.dbc.attributes

    def attr(name: str) -> Any:
        return attributes[name].value if name in attributes else None

    mode = {0: "None", 1: "Cyclic", 2: "Event"}[int(attr("LsE2eMode") or 0)]
    return {
        "frame_id": message.frame_id,
        "dlc": message.length,
        "mode": mode,
        "data_id": int(attr("LsE2eDataId") or 0),
        "max_delta": int(attr("LsE2eMaxDelta") or 0),
        "rx_timeout_ms": int(attr("LsRxTimeoutMs") or 0),
    }


def _payload(message: Any, raw: dict[str, int]) -> bytes:
    values = {signal.name: 0 for signal in message.signals}
    values.update(raw)
    return bytes(message.encode(values, scaling=False, strict=True))


def build_e2e(repo_root: Path = REPO_ROOT) -> dict[str, Any]:
    """Compute the E2E vector document."""
    db = _load_dbc(repo_root)
    messages = {}
    for message in db.messages:
        config = _message_config(message)
        if config["mode"] != "None":
            messages[message.name] = config
    frames = []
    for name, msg_name, content, counter, raw in E2E_FRAMES:
        message = db.get_message_by_name(msg_name)
        config = messages[msg_name]
        payload = _payload(message, raw)
        protected = refimpl.e2e_protect(config["data_id"], payload, counter)
        frames.append(
            {
                "name": name,
                "message": msg_name,
                "content": content,
                "data_id": config["data_id"],
                "counter": counter,
                "signals_raw": raw,
                "payload": payload.hex(),
                "crc_input": refimpl.e2e_crc_input(config["data_id"], protected).hex(),
                "bytes": protected.hex(),
            }
        )
    base = {
        "CGW_WinCmd": {"WinCmd_Req": 0, "WinCmd_PressId": 7, "WinCmd_HoldAge": 0},
        "CGW_DoorCmd": {"DoorCmd_Req": 1, "DoorCmd_ReqId": 0x2A},
        "DCU_TempSts": {"TempSts_Status": 1, "TempSts_Value": 2345, "TempSts_SampleSeq": 42},
    }
    sequences = []
    for name, msg_name, description, steps in RX_SCENARIOS:
        config = messages[msg_name]
        payload = _payload(db.get_message_by_name(msg_name), base[msg_name])
        rx = refimpl.E2eReceiver(
            data_id=config["data_id"],
            dlc=config["dlc"],
            max_delta=config["max_delta"],
            cyclic=config["mode"] == "Cyclic",
        )
        out_steps: list[dict[str, Any]] = []
        for step in steps:
            if step[0] == "timeout":
                rx.timeout()
                out_steps.append({"event": "timeout", "state": rx.state})
                continue
            _, counter, fault = step
            frame = refimpl.e2e_protect(config["data_id"], payload, counter)
            if fault == "crc":
                frame = frame[:2] + bytes((frame[2] ^ 0x01,)) + frame[3:]
            elif fault == "dlc":
                frame = frame[:-1]
            status, delivered = rx.check(frame)
            out_steps.append(
                {
                    "event": "frame",
                    "bytes": frame.hex(),
                    "status": status,
                    "state": rx.state,
                    "delivered": delivered,
                }
            )
        sequences.append(
            {
                "name": name,
                "message": msg_name,
                "description": description,
                "steps": out_steps,
                "counters": dict(rx.counters),
            }
        )
    check_input = b"123456789"
    return {
        "version": 1,
        "source": "LS-SAIC-001 v0.2 sections 7.2, 7.9 and 7.10",
        "generator": "tools/codegen/vectors.py: independent Python reference, cantools 44.1.0 packing",
        "crc": {
            "name": "CRC-8/SAE-J1850",
            "width": 8,
            "poly": refimpl.CRC8_POLY,
            "init": refimpl.CRC8_INIT,
            "xorout": refimpl.CRC8_XOROUT,
            "refin": False,
            "refout": False,
            "check": {"input": check_input.hex(), "crc": refimpl.crc8_j1850(check_input)},
        },
        "profile": {
            "crc_byte": 0,
            "counter_byte": 1,
            "counter_mask": 15,
            "crc_input": "DataID low byte, DataID high byte, frame bytes 1 .. DLC-1",
            "n_ok_valid": 2,
            "n_err_invalid": 3,
        },
        "messages": messages,
        "frames": frames,
        "rx_sequences": sequences,
    }


# --------------------------------------------------------------------------- APP session

K_PAIR = bytes(range(0x20))
DEVICE_ID = bytes.fromhex("0102030405060708")
SERVER_NONCE = bytes(range(0x10, 0x20))
CLIENT_NONCE = bytes(range(0x20, 0x30))
CLIENT_ID = bytes(range(0x30, 0x40))


def _version(major: int, minor: int) -> bytes:
    return refimpl.pb_varint(1, major) + refimpl.pb_varint(2, minor)


def _session_messages(
    proofs: dict[str, bytes],
) -> list[tuple[str, str, int, str, dict[str, Any], bytes]]:
    """(name, direction, counter, Body member, fields, Body bytes) of the session vectors."""
    pb = refimpl
    hello = (
        pb.pb_message(1, _version(1, 0))
        + pb.pb_bytes(2, DEVICE_ID)
        + pb.pb_bytes(3, SERVER_NONCE)
        + pb.pb_bytes(4, b"0.1.0-dev+a1b2c3d")
        + pb.pb_message(5, _version(1, 0))
    )
    auth = (
        pb.pb_message(1, _version(1, 0))
        + pb.pb_bytes(2, CLIENT_ID)
        + pb.pb_bytes(3, CLIENT_NONCE)
        + pb.pb_bytes(4, proofs["client_proof"])
        + pb.pb_bytes(5, b"0.1.0")
    )
    result_ok = (
        pb.pb_varint(1, 1)
        + pb.pb_bytes(2, proofs["server_proof"])
        + pb.pb_varint(3, 0x12345678)
        + pb.pb_varint(4, 3000)
        + pb.pb_varint(5, 100)
        + pb.pb_varint(6, 350)
    )
    result_rejected = pb.pb_varint(1, 11) + pb.pb_bytes(2, bytes(32))
    status = (
        pb.pb_varint(1, 7)
        + pb.pb_varint(2, 1)
        + pb.pb_varint(3, 2)
        + pb.pb_varint(4, 255)
        + pb.pb_sint32(6, -1234)
        + pb.pb_varint(7, 1)
        + pb.pb_varint(8, 2)
        + pb.pb_varint(9, 2)
        + pb.pb_varint(12, 1)
        + pb.pb_varint(13, 124)
        + pb.pb_varint(14, 85)
        + pb.pb_varint(15, 1)
        + pb.pb_varint(16, 1)
        + pb.pb_varint(18, 0x101)
        + pb.pb_sint32(20, -1698)
        + pb.pb_varint(21, 1)
    )
    return [
        (
            "server_hello",
            "cgw_to_app",
            0,
            "server_hello",
            {
                "proto": [1, 0],
                "device_id": DEVICE_ID.hex(),
                "server_nonce": SERVER_NONCE.hex(),
                "fw_version": "0.1.0-dev+a1b2c3d",
                "com_matrix": [1, 0],
                "pairing_window_open": False,
            },
            pb.pb_message(1, hello),
        ),
        (
            "client_auth",
            "app_to_cgw",
            0,
            "client_auth",
            {
                "proto": [1, 0],
                "client_id": CLIENT_ID.hex(),
                "client_nonce": CLIENT_NONCE.hex(),
                "client_proof": proofs["client_proof"].hex(),
                "app_version": "0.1.0",
            },
            pb.pb_message(2, auth),
        ),
        (
            "auth_result_ok",
            "cgw_to_app",
            1,
            "auth_result",
            {
                "result": "COMMAND_RESULT_OK",
                "server_proof": proofs["server_proof"].hex(),
                "session_id": 0x12345678,
                "session_timeout_ms": 3000,
                "keepalive_period_ms": 100,
                "keepalive_timeout_ms": 350,
            },
            pb.pb_message(3, result_ok),
        ),
        (
            "auth_result_rejected",
            "cgw_to_app",
            0,
            "auth_result",
            {"result": "COMMAND_RESULT_REJECTED_AUTH", "server_proof": bytes(32).hex()},
            pb.pb_message(3, result_rejected),
        ),
        (
            "ping_after_auth",
            "cgw_to_app",
            2,
            "ping",
            {"timestamp_ms": 1000},
            pb.pb_message(4, pb.pb_varint(1, 1000)),
        ),
        (
            "window_move_first",
            "app_to_cgw",
            1,
            "window_move",
            {"press_id": 1, "direction": "WINDOW_DIRECTION_UP", "hold_ms": 0},
            pb.pb_message(11, pb.pb_varint(1, 1) + pb.pb_varint(2, 1)),
        ),
        (
            "window_move_keepalive",
            "app_to_cgw",
            2,
            "window_move",
            {"press_id": 1, "direction": "WINDOW_DIRECTION_UP", "hold_ms": 100},
            pb.pb_message(11, pb.pb_varint(1, 1) + pb.pb_varint(2, 1) + pb.pb_varint(3, 100)),
        ),
        (
            "window_stop",
            "app_to_cgw",
            3,
            "window_stop",
            {"press_id": 1},
            pb.pb_message(12, pb.pb_varint(1, 1)),
        ),
        (
            "door_command_unlock",
            "app_to_cgw",
            4,
            "door_command",
            {"request_id": 1, "action": "DOOR_ACTION_UNLOCK"},
            pb.pb_message(10, pb.pb_varint(1, 1) + pb.pb_varint(2, 2)),
        ),
        ("status_request", "app_to_cgw", 5, "status_request", {}, pb.pb_message(13, b"")),
        (
            "pong",
            "cgw_to_app",
            2,
            "pong",
            {"echo_timestamp_ms": 1000},
            pb.pb_message(5, pb.pb_varint(1, 1000)),
        ),
        (
            "command_ack_accepted",
            "cgw_to_app",
            3,
            "command_ack",
            {"kind": "COMMAND_KIND_WINDOW", "ref_id": 1, "result": "COMMAND_RESULT_ACCEPTED"},
            pb.pb_message(20, pb.pb_varint(1, 2) + pb.pb_varint(2, 1) + pb.pb_varint(3, 2)),
        ),
        (
            "door_command_result_ok",
            "cgw_to_app",
            4,
            "door_command_result",
            {
                "request_id": 1,
                "result": "COMMAND_RESULT_OK",
                "lock_state": "DOOR_LOCK_STATE_UNLOCKED",
            },
            pb.pb_message(21, pb.pb_varint(1, 1) + pb.pb_varint(2, 1) + pb.pb_varint(3, 2)),
        ),
        (
            "status_update_negative_values",
            "cgw_to_app",
            5,
            "status_update",
            {
                "seq": 7,
                "door_lock_state": "DOOR_LOCK_STATE_LOCKED",
                "window_state": "WINDOW_STATE_MOVING_UP",
                "window_position_pct": 255,
                "temperature_cdeg": -1234,
                "temp_status": "TEMP_STATUS_VALID",
                "dcu_mode": "NODE_MODE_NORMAL",
                "cgw_mode": "NODE_MODE_NORMAL",
                "dcu_alive": True,
                "vbat_dv": 124,
                "status_age_ms": 85,
                "last_door_request_id": 1,
                "last_door_result": "COMMAND_RESULT_OK",
                "window_fault_flags": 0x101,
                "window_speed_rpm_x10": -1698,
                "window_encoder_status": "ENCODER_STATUS_OK",
            },
            pb.pb_message(22, status),
        ),
        (
            "notice_ka_timeout",
            "cgw_to_app",
            6,
            "notice",
            {"severity": "FAULT_SEVERITY_WARNING", "code": 1, "text": "Keep-alive timeout"},
            pb.pb_message(
                23, pb.pb_varint(1, 1) + pb.pb_varint(2, 1) + pb.pb_bytes(3, b"Keep-alive timeout")
            ),
        ),
        (
            "session_close_timeout",
            "cgw_to_app",
            7,
            "session_close",
            {"reason": "COMMAND_RESULT_FAILED_TIMEOUT"},
            pb.pb_message(24, pb.pb_varint(1, 9)),
        ),
    ]


def build_app_session() -> dict[str, Any]:
    """Compute the APP session vector document."""
    pb = refimpl
    proofs = {
        "client_proof": pb.client_proof(K_PAIR, DEVICE_ID, SERVER_NONCE, CLIENT_NONCE, CLIENT_ID),
        "server_proof": pb.server_proof(K_PAIR, DEVICE_ID, SERVER_NONCE, CLIENT_NONCE, CLIENT_ID),
    }
    salt = SERVER_NONCE + CLIENT_NONCE
    info = pb.LABEL_SESSION + DEVICE_ID + CLIENT_ID
    prk, k_sess = pb.hkdf_sha256(K_PAIR, salt, info, 32)
    directions = {"app_to_cgw": pb.DIR_APP_TO_CGW, "cgw_to_app": pb.DIR_CGW_TO_APP}
    frames: list[dict[str, Any]] = []
    for name, direction, counter, member, fields, body in _session_messages(proofs):
        handshake = counter == 0
        tag = b"" if handshake else pb.frame_tag(k_sess, directions[direction], counter, body)
        frame = pb.pb_varint(1, counter) + pb.pb_bytes(2, body) + pb.pb_bytes(3, tag)
        frames.append(
            {
                "name": name,
                "direction": direction,
                "counter": counter,
                "body_member": member,
                "fields": fields,
                "body": body.hex(),
                "tag": tag.hex(),
                "frame": frame.hex(),
            }
        )
    first = next(f for f in frames if f["name"] == "window_move_first")
    wrong_dir_tag = pb.frame_tag(k_sess, pb.DIR_CGW_TO_APP, 1, bytes.fromhex(first["body"]))
    return {
        "version": 1,
        "source": "LS-SAIC-001 v0.2 sections 8.2, 8.3 and 8.6",
        "generator": "tools/codegen/vectors.py: Python hmac/hashlib, RFC 5869 HKDF, hand-encoded proto3",
        "note": (
            "Each frame is an independent example. Handshake frames (counter 0) carry an empty tag. "
            "Bodies are canonical proto3 encodings in field-number order; fixed-length bytes fields "
            "are always present, as nanopb encodes them."
        ),
        "inputs": {
            "k_pair": K_PAIR.hex(),
            "device_id": DEVICE_ID.hex(),
            "server_nonce": SERVER_NONCE.hex(),
            "client_nonce": CLIENT_NONCE.hex(),
            "client_id": CLIENT_ID.hex(),
        },
        "labels": {
            "client_proof": pb.LABEL_CLIENT.hex(),
            "server_proof": pb.LABEL_SERVER.hex(),
            "session": pb.LABEL_SESSION.hex(),
        },
        "directions": {"app_to_cgw": pb.DIR_APP_TO_CGW, "cgw_to_app": pb.DIR_CGW_TO_APP},
        "tag_length": pb.TAG_LEN,
        "client_proof": proofs["client_proof"].hex(),
        "server_proof": proofs["server_proof"].hex(),
        "hkdf": {
            "ikm": K_PAIR.hex(),
            "salt": salt.hex(),
            "info": info.hex(),
            "length": 32,
            "prk": prk.hex(),
            "okm": k_sess.hex(),
        },
        "k_sess": k_sess.hex(),
        "frames": frames,
        "negative": [
            {
                "name": "tag_with_wrong_direction",
                "description": "Body of window_move_first (counter 1) tagged with dir 0x43; must not verify as APP -> CGW",
                "direction_used": "cgw_to_app",
                "counter": 1,
                "body": first["body"],
                "tag": wrong_dir_tag.hex(),
                "expect": "reject",
            },
            {
                "name": "counter_mismatch",
                "description": "Tag of window_move_first presented with counter 2; must not verify",
                "direction_used": "app_to_cgw",
                "counter": 2,
                "body": first["body"],
                "tag": first["tag"],
                "expect": "reject",
            },
        ],
    }


# --------------------------------------------------------------------------- known answers


def build_crypto_kat() -> dict[str, Any]:
    """RFC 4231 (HMAC-SHA256) and RFC 5869 (HKDF-SHA256) test cases, recomputed."""
    hmac_cases = [
        ("rfc4231_tc1", bytes([0x0B] * 20), b"Hi There", 32),
        ("rfc4231_tc2", b"Jefe", b"what do ya want for nothing?", 32),
        ("rfc4231_tc3", bytes([0xAA] * 20), bytes([0xDD] * 50), 32),
        ("rfc4231_tc4", bytes(range(0x01, 0x1A)), bytes([0xCD] * 50), 32),
        ("rfc4231_tc5", bytes([0x0C] * 20), b"Test With Truncation", 16),
        (
            "rfc4231_tc6",
            bytes([0xAA] * 131),
            b"Test Using Larger Than Block-Size Key - Hash Key First",
            32,
        ),
        (
            "rfc4231_tc7",
            bytes([0xAA] * 131),
            b"This is a test using a larger than block-size key and a larger than block-size data. "
            b"The key needs to be hashed before being used by the HMAC algorithm.",
            32,
        ),
    ]
    hkdf_cases = [
        ("rfc5869_tc1", bytes([0x0B] * 22), bytes(range(0x0D)), bytes(range(0xF0, 0xFA)), 42),
        (
            "rfc5869_tc2",
            bytes(range(0x50)),
            bytes(range(0x60, 0xB0)),
            bytes(range(0xB0, 0x100)),
            82,
        ),
        ("rfc5869_tc3", bytes([0x0B] * 22), b"", b"", 42),
    ]
    return {
        "version": 1,
        "source": "RFC 4231 section 4 (HMAC-SHA-256 results) and RFC 5869 appendix A.1-A.3",
        "generator": "tools/codegen/vectors.py: Python hmac/hashlib",
        "hmac_sha256": [
            {
                "name": name,
                "key": key.hex(),
                "data": data.hex(),
                "truncate_bytes": length,
                "mac": refimpl.hmac_sha256(key, data)[:length].hex(),
            }
            for name, key, data, length in hmac_cases
        ],
        "hkdf_sha256": [
            {
                "name": name,
                "ikm": ikm.hex(),
                "salt": salt.hex(),
                "info": info.hex(),
                "length": length,
                "prk": refimpl.hkdf_sha256(ikm, salt, info, length)[0].hex(),
                "okm": refimpl.hkdf_sha256(ikm, salt, info, length)[1].hex(),
            }
            for name, ikm, salt, info, length in hkdf_cases
        ],
    }


#: Published RFC values (not recomputed) that anchor the known-answer file.
RFC_ANCHORS = {
    "rfc4231_tc1": "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",
    "rfc4231_tc2": "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
    "rfc5869_tc1": "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865",
}


# --------------------------------------------------------------------------- checks


def dump(document: dict[str, Any]) -> str:
    """Canonical JSON text of a vector document."""
    return json.dumps(document, indent=2, ensure_ascii=False) + "\n"


def contract_problems(e2e: dict[str, Any], session: dict[str, Any], doc: str) -> list[str]:
    """Compare computed vectors with the tables of LS-SAIC-001 sections 7.10 and 8.6."""
    problems = []
    rows = saic.parse_e2e_vectors(doc)
    if not rows or rows[0][3] != bytes((e2e["crc"]["check"]["crc"],)):
        problems.append("LS-SAIC 7.10: CRC check value differs from the reference")
    if len(rows) - 1 != len(e2e["frames"]):
        problems.append(f"LS-SAIC 7.10: {len(rows) - 1} frame rows, {len(e2e['frames'])} vectors")
    for row, frame in zip(rows[1:], e2e["frames"], strict=False):
        expected = (frame["message"], frame["data_id"], bytes.fromhex(frame["bytes"]))
        if (row[0], row[2], row[3]) != expected:
            problems.append(
                f"LS-SAIC 7.10 {row[0]} '{row[1]}': contract {row[3].hex(' ')}, reference {frame['bytes']}"
            )
    table = saic.parse_session_vectors(doc)

    def value(item: str) -> str:
        return table.get(item, "").replace("`", "")

    if value("client_proof") != session["client_proof"]:
        problems.append("LS-SAIC 8.6: client_proof differs from the reference")
    if value("server_proof") != session["server_proof"]:
        problems.append("LS-SAIC 8.6: server_proof differs from the reference")
    if value("K_sess") != session["k_sess"]:
        problems.append("LS-SAIC 8.6: K_sess differs from the reference")
    by_body = {(f["direction"], f["counter"], f["body"]): f["tag"] for f in session["frames"]}
    for item, text in table.items():
        parts = {
            key: val.strip("` ")
            for key, val in (p.strip().split(" ", 1) for p in text.split(",") if " " in p.strip())
        }
        if "body" not in parts:
            continue
        direction = "app_to_cgw" if item.startswith("APP") else "cgw_to_app"
        counter = int(item.split("ctr ")[1].split(",")[0])
        tag = by_body.get((direction, counter, parts["body"]))
        if tag != parts.get("tag"):
            problems.append(
                f"LS-SAIC 8.6 '{item}': contract tag {parts.get('tag')}, reference {tag}"
            )
    negative = next((v for k, v in table.items() if k.startswith("Negative")), "")
    if session["negative"][0]["tag"] not in negative:
        problems.append("LS-SAIC 8.6: negative vector differs from the reference")
    return problems


def protobuf_problems(session: dict[str, Any], pb2: Any) -> list[str]:
    """Encode every session message with the protobuf runtime and compare the Body bytes."""
    problems = []
    for frame in session["frames"]:
        body = pb2.Body()
        sub = getattr(body, frame["body_member"])
        sub.SetInParent()
        for name, val in frame["fields"].items():
            target = getattr(sub, name)
            if name in ("proto", "com_matrix"):
                target.major, target.minor = val
            elif isinstance(val, str) and name in (
                "device_id",
                "server_nonce",
                "client_id",
                "client_nonce",
                "client_proof",
                "server_proof",
            ):
                setattr(sub, name, bytes.fromhex(val))
            elif isinstance(val, str) and val.isupper():
                setattr(
                    sub,
                    name,
                    pb2.DESCRIPTOR.enum_types_by_name[
                        sub.DESCRIPTOR.fields_by_name[name].enum_type.name
                    ]
                    .values_by_name[val]
                    .number,
                )
            else:
                setattr(sub, name, val)
        encoded = body.SerializeToString().hex()
        if encoded != frame["body"]:
            problems.append(
                f"{frame['name']}: protobuf runtime encodes {encoded}, vector {frame['body']}"
            )
        wire = pb2.Frame(
            counter=frame["counter"],
            body=bytes.fromhex(frame["body"]),
            tag=bytes.fromhex(frame["tag"]),
        )
        if wire.SerializeToString().hex() != frame["frame"]:
            problems.append(f"{frame['name']}: Frame encoding differs")
    return problems


def check_files(repo_root: Path = REPO_ROOT, doc: str | None = None) -> list[str]:
    """Verify the committed vector files against the reference and the contract."""
    problems = []
    built = {
        E2E_JSON: build_e2e(repo_root),
        SESSION_JSON: build_app_session(),
        KAT_JSON: build_crypto_kat(),
    }
    for path, document in built.items():
        target = repo_root / path
        if not target.is_file():
            problems.append(f"{path}: missing (run tools/codegen/vectors.py --write)")
        elif target.read_text(encoding="utf-8") != dump(document):
            problems.append(f"{path}: differs from the reference computation")
    kat = {c["name"]: c for c in built[KAT_JSON]["hmac_sha256"] + built[KAT_JSON]["hkdf_sha256"]}
    for name, anchor in RFC_ANCHORS.items():
        if kat[name].get("mac", kat[name].get("okm")) != anchor:
            problems.append(f"{KAT_JSON}: {name} does not reproduce the published RFC value")
    if doc is not None:
        problems += contract_problems(built[E2E_JSON], built[SESSION_JSON], doc)
    return problems


def write_files(repo_root: Path = REPO_ROOT) -> list[Path]:
    """Rewrite the vector files from the reference computation."""
    written = []
    for path, document in (
        (E2E_JSON, build_e2e(repo_root)),
        (SESSION_JSON, build_app_session()),
        (KAT_JSON, build_crypto_kat()),
    ):
        write_text(repo_root / path, dump(document))
        written.append(repo_root / path)
    return written


def main(argv: list[str] | None = None) -> int:
    """Command-line entry point."""
    parser = argparse.ArgumentParser(description="Build or verify interfaces/vectors/*.json.")
    parser.add_argument("--write", action="store_true", help="rewrite the vector files")
    args = parser.parse_args(argv)
    try:
        if args.write:
            for path in write_files():
                print(f"wrote {rel(path)}")
            return 0
        problems = check_files(doc=saic.load(REPO_ROOT))
    except CodegenError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    for problem in problems:
        print(f"error: {problem}", file=sys.stderr)
    if not problems:
        print("vectors: OK")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
