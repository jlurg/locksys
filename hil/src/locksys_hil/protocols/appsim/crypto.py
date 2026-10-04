# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Session cryptography of the APP protocol 1.0 (LS-SAIC-001 section 8.3).

HMAC-SHA256 from the standard library and HKDF-SHA256 per RFC 5869. All comparisons of
secret-dependent values use ``hmac.compare_digest``.
"""

import hashlib
import hmac
from enum import IntEnum
from typing import Final

LABEL_CLIENT_PROOF: Final = b"LSv1|cli"
LABEL_SERVER_PROOF: Final = b"LSv1|srv"
LABEL_SESSION: Final = b"LSv1|session"
TAG_LENGTH: Final = 16
KEY_LENGTH: Final = 32
DEVICE_ID_LENGTH: Final = 8
NONCE_LENGTH: Final = 16
CLIENT_ID_LENGTH: Final = 16


class Direction(IntEnum):
    """Direction byte of the frame tag."""

    APP_TO_CGW = 0x41
    CGW_TO_APP = 0x43


def hmac_sha256(key: bytes, data: bytes) -> bytes:
    """Return HMAC-SHA256(key, data)."""
    return hmac.new(key, data, hashlib.sha256).digest()


def hkdf_extract(salt: bytes, ikm: bytes) -> bytes:
    """RFC 5869 extract step; an empty salt is replaced by 32 zero bytes."""
    return hmac_sha256(salt or bytes(hashlib.sha256().digest_size), ikm)


def hkdf_expand(prk: bytes, info: bytes, length: int) -> bytes:
    """RFC 5869 expand step.

    Raises:
        ValueError: ``length`` exceeds 255 hash blocks.
    """
    if length > 255 * hashlib.sha256().digest_size:
        raise ValueError("HKDF output too long")
    okm = b""
    block = b""
    counter = 1
    while len(okm) < length:
        block = hmac_sha256(prk, block + info + bytes((counter,)))
        okm += block
        counter += 1
    return okm[:length]


def hkdf_sha256(ikm: bytes, salt: bytes, info: bytes, length: int) -> bytes:
    """HKDF-SHA256 (extract then expand)."""
    return hkdf_expand(hkdf_extract(salt, ikm), info, length)


def client_proof(
    k_pair: bytes, device_id: bytes, server_nonce: bytes, client_nonce: bytes, client_id: bytes
) -> bytes:
    """HMAC-SHA256(K_pair, "LSv1|cli" || device_id || server_nonce || client_nonce || client_id)."""
    return hmac_sha256(
        k_pair, LABEL_CLIENT_PROOF + device_id + server_nonce + client_nonce + client_id
    )


def server_proof(
    k_pair: bytes, device_id: bytes, server_nonce: bytes, client_nonce: bytes, client_id: bytes
) -> bytes:
    """HMAC-SHA256(K_pair, "LSv1|srv" || device_id || client_nonce || server_nonce || client_id)."""
    return hmac_sha256(
        k_pair, LABEL_SERVER_PROOF + device_id + client_nonce + server_nonce + client_id
    )


def session_key(
    k_pair: bytes, device_id: bytes, server_nonce: bytes, client_nonce: bytes, client_id: bytes
) -> bytes:
    """K_sess = HKDF-SHA256(IKM=K_pair, salt=server_nonce||client_nonce, info=label||ids, L=32)."""
    return hkdf_sha256(
        k_pair, server_nonce + client_nonce, LABEL_SESSION + device_id + client_id, KEY_LENGTH
    )


def frame_tag(k_sess: bytes, direction: Direction, counter: int, body: bytes) -> bytes:
    """First 16 bytes of HMAC-SHA256(K_sess, dir || counter_be32 || body)."""
    data = bytes((int(direction),)) + counter.to_bytes(4, "big") + body
    return hmac_sha256(k_sess, data)[:TAG_LENGTH]


def verify_tag(k_sess: bytes, direction: Direction, counter: int, body: bytes, tag: bytes) -> bool:
    """Constant-time check of a frame tag over the raw body bytes."""
    return hmac.compare_digest(frame_tag(k_sess, direction, counter, body), tag)
