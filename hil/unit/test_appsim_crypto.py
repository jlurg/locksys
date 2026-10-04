# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Session cryptography and frame codec against the crypto KATs and the session vectors."""

from typing import Any

import pytest

from locksys_hil.gen import locksys_app_pb2 as pb
from locksys_hil.protocols.appsim import crypto
from locksys_hil.protocols.appsim.codec import (
    APP_TO_CGW_MEMBERS,
    CGW_TO_APP_MEMBERS,
    IntegrityError,
    SessionChannel,
    decode_handshake,
    encode_handshake,
    member,
)


def _b(text: str) -> bytes:
    return bytes.fromhex(text)


def test_hmac_kat(crypto_kat: Any) -> None:
    for case in crypto_kat["hmac_sha256"]:
        mac = crypto.hmac_sha256(_b(case["key"]), _b(case["data"]))
        length = case["truncate_bytes"] or len(mac)
        assert mac[:length].hex() == case["mac"], case["name"]


def test_hkdf_kat(crypto_kat: Any) -> None:
    for case in crypto_kat["hkdf_sha256"]:
        prk = crypto.hkdf_extract(_b(case["salt"]), _b(case["ikm"]))
        assert prk.hex() == case["prk"], case["name"]
        okm = crypto.hkdf_sha256(
            _b(case["ikm"]), _b(case["salt"]), _b(case["info"]), case["length"]
        )
        assert okm.hex() == case["okm"], case["name"]


def test_hkdf_length_limit() -> None:
    with pytest.raises(ValueError, match="too long"):
        crypto.hkdf_expand(bytes(32), b"", 255 * 32 + 1)


def _inputs(v: Any) -> tuple[bytes, bytes, bytes, bytes, bytes]:
    i = v["inputs"]
    return (
        _b(i["k_pair"]),
        _b(i["device_id"]),
        _b(i["server_nonce"]),
        _b(i["client_nonce"]),
        _b(i["client_id"]),
    )


def test_labels(session_vectors: Any) -> None:
    labels = session_vectors["labels"]
    assert crypto.LABEL_CLIENT_PROOF.hex() == labels["client_proof"]
    assert crypto.LABEL_SERVER_PROOF.hex() == labels["server_proof"]
    assert crypto.LABEL_SESSION.hex() == labels["session"]
    directions = session_vectors["directions"]
    assert directions["app_to_cgw"] == crypto.Direction.APP_TO_CGW
    assert directions["cgw_to_app"] == crypto.Direction.CGW_TO_APP


def test_proofs_and_session_key(session_vectors: Any) -> None:
    args = _inputs(session_vectors)
    assert crypto.client_proof(*args).hex() == session_vectors["client_proof"]
    assert crypto.server_proof(*args).hex() == session_vectors["server_proof"]
    assert crypto.session_key(*args).hex() == session_vectors["k_sess"]
    hkdf = session_vectors["hkdf"]
    assert crypto.hkdf_extract(_b(hkdf["salt"]), _b(hkdf["ikm"])).hex() == hkdf["prk"]


def _direction(name: str) -> crypto.Direction:
    return crypto.Direction.APP_TO_CGW if name == "app_to_cgw" else crypto.Direction.CGW_TO_APP


def test_frames(session_vectors: Any) -> None:
    k_sess = _b(session_vectors["k_sess"])
    for vector in session_vectors["frames"]:
        frame = pb.Frame()
        frame.ParseFromString(_b(vector["frame"]))
        assert frame.counter == vector["counter"], vector["name"]
        assert frame.body.hex() == vector["body"], vector["name"]
        assert frame.tag.hex() == vector["tag"], vector["name"]
        body = pb.Body()
        body.ParseFromString(frame.body)
        assert member(body) == vector["body_member"], vector["name"]
        assert body.SerializeToString() == frame.body, vector["name"]
        direction = _direction(vector["direction"])
        allowed = (
            APP_TO_CGW_MEMBERS if direction is crypto.Direction.APP_TO_CGW else CGW_TO_APP_MEMBERS
        )
        assert vector["body_member"] in allowed
        if vector["counter"] == 0:
            assert encode_handshake(body).hex() == vector["frame"], vector["name"]
            assert decode_handshake(frame.SerializeToString(), allowed) == body
            continue
        channel = SessionChannel(
            k_sess=k_sess, tx_direction=direction, tx_counter=frame.counter - 1
        )
        assert channel.seal(body).hex() == vector["frame"], vector["name"]
        receiver_direction = (
            crypto.Direction.CGW_TO_APP
            if direction is crypto.Direction.APP_TO_CGW
            else crypto.Direction.APP_TO_CGW
        )
        receiver = SessionChannel(k_sess=k_sess, tx_direction=receiver_direction)
        assert receiver.open(_b(vector["frame"])) == body


def test_negative_vectors(session_vectors: Any) -> None:
    k_sess = _b(session_vectors["k_sess"])
    for vector in session_vectors["negative"]:
        assert vector["expect"] == "reject"
        assert not crypto.verify_tag(
            k_sess,
            crypto.Direction.APP_TO_CGW,
            vector["counter"],
            _b(vector["body"]),
            _b(vector["tag"]),
        ), vector["name"]
        raw = pb.Frame(
            counter=vector["counter"], body=_b(vector["body"]), tag=_b(vector["tag"])
        ).SerializeToString()
        cgw = SessionChannel(k_sess=k_sess, tx_direction=crypto.Direction.CGW_TO_APP)
        with pytest.raises(IntegrityError, match="bad tag"):
            cgw.open(raw)


def test_session_rules() -> None:
    k_sess = bytes(range(32))
    app = SessionChannel(k_sess=k_sess, tx_direction=crypto.Direction.APP_TO_CGW)
    cgw = SessionChannel(k_sess=k_sess, tx_direction=crypto.Direction.CGW_TO_APP)
    first = app.seal(pb.Body(window_stop=pb.WindowStop(press_id=1)))
    assert member(cgw.open(first)) == "window_stop"
    with pytest.raises(IntegrityError, match="counter"):
        cgw.open(first)  # replay
    wrong_direction = app.seal(pb.Body(status_update=pb.StatusUpdate(seq=1)))
    with pytest.raises(IntegrityError, match="not allowed"):
        cgw.open(wrong_direction)
    with pytest.raises(IntegrityError, match="exceeds"):
        cgw.open(bytes(257))
    with pytest.raises(IntegrityError, match="malformed"):
        cgw.open(b"\xff\xff\xff")
    unknown = app.seal(pb.Body())
    assert member(cgw.open(unknown)) is None


def test_handshake_rules() -> None:
    hello = encode_handshake(pb.Body(server_hello=pb.ServerHello(fw_version="x")))
    with pytest.raises(IntegrityError, match="unexpected"):
        decode_handshake(hello, frozenset({"client_auth"}))
    tagged = pb.Frame(counter=1, body=b"", tag=bytes(16)).SerializeToString()
    with pytest.raises(IntegrityError, match="counter 0"):
        decode_handshake(tagged, frozenset({"server_hello"}))
