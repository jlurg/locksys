# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""``locksys-cgw-sim`` command line: run the simulator.

The pairing payload is printed in the simulator form (with the ``h`` parameter) so that the APP
can pair from it. The simulator key is not a product secret.
"""

import argparse
import asyncio
import base64
import contextlib
import hashlib
import logging
import secrets
import sys
from collections.abc import Sequence

from locksys_cgw_sim.core import CgwSimCore, SimConfig
from locksys_cgw_sim.plant import Plant
from locksys_cgw_sim.server import SimServer


def pairing_payload(config: SimConfig, host: str, port: int) -> str:
    """Return the simulator pairing payload ``locksys://pair?...&h=<host:port>``."""
    key = base64.urlsafe_b64encode(config.k_pair).rstrip(b"=").decode()
    return (
        f"locksys://pair?v=1&id={config.device_id.hex().upper()}&s=LockSys-SIM0"
        f"&p=SIMULATORSIMULATOR00&k={key}&b=02:00:00:00:53:30&sec=wpa3&h={host}:{port}"
    )


async def _serve(config: SimConfig, host: str, port: int, seed: int) -> None:
    server = SimServer(CgwSimCore(config, Plant(seed=seed)))
    await server.start(host, port)
    print(pairing_payload(config, host, server.port), flush=True)
    try:
        await asyncio.Future()
    finally:
        await server.stop()


def main(argv: Sequence[str] | None = None) -> int:
    """Parse arguments and run the simulator until interrupted."""
    parser = argparse.ArgumentParser(prog="locksys-cgw-sim", description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--k-pair", help="pairing key as 64 hex digits (default: random)")
    parser.add_argument("--fw-version", default="0.1.0-dev+sim")
    parser.add_argument("--seed", type=int, default=1, help="plant model random seed")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args(argv)
    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)
    k_pair = bytes.fromhex(args.k_pair) if args.k_pair else secrets.token_bytes(32)
    if len(k_pair) != 32:
        parser.error("--k-pair must be 32 bytes")
    device_id = hashlib.sha256(b"locksys-cgw-sim" + k_pair[:4]).digest()[:8]
    config = SimConfig(k_pair=k_pair, device_id=device_id, fw_version=args.fw_version)
    with contextlib.suppress(KeyboardInterrupt):
        asyncio.run(_serve(config, args.host, args.port, args.seed))
    return 0


if __name__ == "__main__":
    sys.exit(main())
