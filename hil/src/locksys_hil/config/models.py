# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Bench configuration model (``hil/config/benches/*.yaml``).

Devices are identified by USB serial number or instrument identity, never by COM port number
(LS-HIL-001 section 3.1). Secrets are never part of the configuration; they live in the OS
keyring under the names given in ``secrets``.
"""

from typing import Literal

from pydantic import BaseModel, ConfigDict, Field

Capability = Literal[
    "dcu",
    "cgw",
    "can",
    "psu.kl30",
    "psu.kl30_sense",
    "dmm",
    "scope",
    "logic.16ch",
    "logic.8ch",
    "stimulus",
    "wifi",
    "usb_power_switch",
    "can_short_relay",
]
"""Capabilities a test can require with ``@pytest.mark.requires``."""


class _Model(BaseModel):
    model_config = ConfigDict(extra="forbid", frozen=True)


class SerialRef(_Model):
    """USB serial device located by USB serial number (and optionally interface number)."""

    usb_serial: str = Field(description="USB serial number of the device")
    interface: int | None = Field(default=None, description="USB interface number (composite)")
    baudrate: int = 115200


class DcuConfig(_Model):
    """NUCLEO-F103RB DUT."""

    stlink_serial: str = Field(description="ST-LINK serial (flashing and VCP)")
    vcp: SerialRef
    lock_en_diag_option: Literal["A", "B"] = "A"


class CgwConfig(_Model):
    """ESP32-S3-DevKitC-1 DUT."""

    console: SerialRef
    ws_url: str = "ws://192.168.4.1:80/ws/v1"


class CanConfig(_Model):
    """USB-CAN adapter (python-can)."""

    interface: str = Field(description="python-can interface name, e.g. pcan or virtual")
    channel: str
    device_id: str | None = Field(default=None, description="Adapter device ID")
    bitrate: int = 500000


class VisaInstrument(_Model):
    """SCPI instrument on VISA."""

    driver: Literal["scpi", "fake"] = "scpi"
    resource: str = Field(description="VISA resource string")
    backend: str = "@py"
    identity: str | None = Field(default=None, description="Expected *IDN? serial number")


class PsuConfig(VisaInstrument):
    """Programmable supply: CH1 actuator supply (KL30), CH2 KL30-sense emulation."""

    kl30_channel: int = 1
    kl30_sense_channel: int | None = 2
    kl30_current_limit_a: float = Field(default=3.0, le=3.0)


class LogicConfig(_Model):
    """Saleae Logic 2 automation endpoint."""

    driver: Literal["saleae", "fake"] = "saleae"
    host: Literal["127.0.0.1"] = "127.0.0.1"
    port: int = 10430
    device_id: str | None = None
    channels: Literal[8, 16] = 16


class StimulusConfig(_Model):
    """Stimulus MCU (control and event CDC interfaces)."""

    driver: Literal["serial", "fake"] = "serial"
    control: SerialRef | None = None
    events: SerialRef | None = None
    protocol_major: int = 1


class WifiConfig(_Model):
    """WLAN-DUT adapter used by the APP simulator."""

    interface: str = Field(description="OS interface alias of the WPA3 adapter")
    local_addr: str | None = Field(default=None, description="Address bound by the APP simulator")


class SecretNames(_Model):
    """Keyring entry names (values are never stored in files)."""

    keyring_service: str = "locksys-hil"
    k_pair: str = "k_pair"
    client_id: str = "client_id"
    wifi_passphrase: str = "wifi_passphrase"


class BenchConfig(_Model):
    """Complete description of one bench."""

    schema_version: Literal[1] = 1
    name: str
    configuration: Literal["SIM", "REAL"] = "SIM"
    lock_file: str = Field(
        description="Framework bench lock (filelock); must be writable by the runner account"
    )
    evidence_dir: str = "hil/reports"
    capabilities: list[Capability] = Field(default_factory=list)
    dcu: DcuConfig | None = None
    cgw: CgwConfig | None = None
    can: CanConfig | None = None
    psu: PsuConfig | None = None
    dmm: VisaInstrument | None = None
    logic: LogicConfig | None = None
    stimulus: StimulusConfig | None = None
    wifi: WifiConfig | None = None
    secrets: SecretNames = Field(default_factory=SecretNames)
