# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""UDS-lite client over ISO-TP (LS-SAIC-001 section 7.8): udsoncan 1.26.1 on can-isotp 2.0.7.

The transport uses ``NotifierBasedCanStack`` with 0xCC padding to 8 bytes. The client is
created on demand so that importing the module needs no CAN hardware.
"""

from dataclasses import dataclass
from typing import Any, Final

REQUEST_ID: Final = 0x7A0
"""Physical request identifier (DIAG_DcuReq)."""
RESPONSE_ID: Final = 0x7A8
"""Response identifier (DIAG_DcuResp)."""

DID_ECU_SERIAL: Final = 0xF18C
DID_SW_VERSION: Final = 0xF195
DID_COM_MATRIX: Final = 0xFD00
DID_TEMPERATURE: Final = 0xFD01
DID_KL30: Final = 0xFD02
DID_WINDOW: Final = 0xFD03
DID_LOCK: Final = 0xFD04
DID_RESET_HISTORY: Final = 0xFD05
DID_STACK: Final = 0xFD06
DID_CPU_LOAD: Final = 0xFD07
DID_E2E_COUNTERS: Final = 0xFD08
DID_SM_TRACE: Final = 0xFD09


@dataclass(frozen=True)
class IsoTpSettings:
    """ISO-TP parameters of the tester (LS-SAIC-001 section 5.0.6)."""

    stmin_ms: int = 5
    blocksize: int = 0
    tx_padding: int = 0xCC
    tx_data_min_length: int = 8
    rx_flowcontrol_timeout_ms: int = 1000
    rx_consecutive_frame_timeout_ms: int = 1000


def create_client(bus: Any, settings: IsoTpSettings | None = None, timeout_s: float = 1.0) -> Any:
    """Create a udsoncan client on a python-can bus.

    Args:
        bus: python-can bus instance.
        settings: ISO-TP parameters.
        timeout_s: P2 request time-out.

    Returns:
        An unopened ``udsoncan.client.Client``; use it as a context manager.
    """
    import can
    import isotp
    from udsoncan.client import Client
    from udsoncan.connections import PythonIsoTpConnection

    cfg = settings or IsoTpSettings()
    notifier = can.Notifier(bus, [])
    stack = isotp.NotifierBasedCanStack(
        bus,
        notifier,
        address=isotp.Address(
            isotp.AddressingMode.Normal_11bits, txid=REQUEST_ID, rxid=RESPONSE_ID
        ),
        params={
            "stmin": cfg.stmin_ms,
            "blocksize": cfg.blocksize,
            "tx_padding": cfg.tx_padding,
            "tx_data_min_length": cfg.tx_data_min_length,
            "rx_flowcontrol_timeout": cfg.rx_flowcontrol_timeout_ms,
            "rx_consecutive_frame_timeout": cfg.rx_consecutive_frame_timeout_ms,
        },
    )
    return Client(PythonIsoTpConnection(stack), request_timeout=timeout_s)
