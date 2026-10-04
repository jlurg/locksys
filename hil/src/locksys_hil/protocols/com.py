# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""CAN codec on the normative DBC ``interfaces/can/locksys.dbc`` (cantools 44.1.0).

Encoding works on raw signal values (no scaling) so that tests compare enum codes and fault
frames byte for byte; E2E protection is applied per transmission from the DBC attributes
``LsE2eMode``, ``LsE2eDataId`` and ``LsE2eMaxDelta``.
"""

from collections.abc import Mapping
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import cantools

from locksys_hil.paths import dbc_file
from locksys_hil.protocols.e2e import E2eMode, E2eReceiver, protect

CRC_SUFFIX = "_Crc"
COUNTER_SUFFIX = "_AliveCtr"


@dataclass(frozen=True)
class E2eSpec:
    """E2E configuration of one message from the DBC attributes."""

    mode: E2eMode
    data_id: int
    max_delta: int
    rx_timeout_ms: int


class ComCodec:
    """Encoder and decoder for the LockSys CAN matrix."""

    def __init__(self, dbc_path: Path | None = None) -> None:
        """Load the DBC (default: the repository DBC)."""
        self._db: Any = cantools.database.load_file(
            str(dbc_path or dbc_file()), database_format="dbc", strict=True
        )

    @property
    def database(self) -> Any:
        """The underlying cantools database."""
        return self._db

    def message(self, name: str) -> Any:
        """Return the cantools message ``name``."""
        return self._db.get_message_by_name(name)

    def frame_id(self, name: str) -> int:
        """Return the CAN identifier of ``name``."""
        return int(self.message(name).frame_id)

    def _attribute(self, name: str, attribute: str) -> Any:
        message = self.message(name)
        attributes = message.dbc.attributes
        if attribute in attributes:
            return attributes[attribute].value
        return self._db.dbc.attribute_definitions[attribute].default_value

    def cycle_time_ms(self, name: str) -> int:
        """Return ``GenMsgCycleTime`` of ``name`` (0 for event messages)."""
        return int(self._attribute(name, "GenMsgCycleTime"))

    def e2e_spec(self, name: str) -> E2eSpec | None:
        """Return the E2E configuration of ``name``, or None for unprotected messages."""
        raw_mode = self._attribute(name, "LsE2eMode")
        definition = self._db.dbc.attribute_definitions["LsE2eMode"]
        mode_name = definition.choices[raw_mode] if isinstance(raw_mode, int) else str(raw_mode)
        if mode_name == "None":
            return None
        return E2eSpec(
            mode=E2eMode(mode_name),
            data_id=int(self._attribute(name, "LsE2eDataId")),
            max_delta=int(self._attribute(name, "LsE2eMaxDelta")),
            rx_timeout_ms=int(self._attribute(name, "LsRxTimeoutMs")),
        )

    def receiver(self, name: str) -> E2eReceiver:
        """Return an E2E receiver configured for ``name``.

        Raises:
            ValueError: the message is not E2E protected.
        """
        spec = self.e2e_spec(name)
        if spec is None:
            raise ValueError(f"{name} is not E2E protected")
        return E2eReceiver(
            data_id=spec.data_id,
            dlc=int(self.message(name).length),
            mode=spec.mode,
            max_delta=spec.max_delta,
        )

    def senders(self, node: str) -> list[str]:
        """Return the names of all messages transmitted by ``node``."""
        return [m.name for m in self._db.messages if node in m.senders]

    def encode_raw(
        self, name: str, signals: Mapping[str, int], counter: int | None = None
    ) -> bytes:
        """Encode raw signal values; missing signals are 0.

        Args:
            name: Message name.
            signals: Raw values (no scaling); CRC and alive counter are ignored.
            counter: Alive counter for E2E messages; None leaves byte 0 and the counter as given.

        Returns:
            The frame bytes, E2E protected when the message has an E2E mode and a counter is given.
        """
        message = self.message(name)
        values = {signal.name: int(signals.get(signal.name, 0)) for signal in message.signals}
        data = bytes(message.encode(values, scaling=False, strict=True))
        spec = self.e2e_spec(name)
        if spec is not None and counter is not None:
            data = protect(data, spec.data_id, counter)
        return data

    def decode(self, frame_id: int, data: bytes, *, raw: bool = True) -> dict[str, Any]:
        """Decode a frame; ``raw`` returns raw integers without choices or scaling."""
        message = self._db.get_message_by_frame_id(frame_id)
        decoded = message.decode(data, decode_choices=not raw, scaling=not raw)
        return dict(decoded)
