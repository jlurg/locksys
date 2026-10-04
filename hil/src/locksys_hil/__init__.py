# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""LockSys hardware-in-the-loop framework (LS-HIL-001).

Layers: ``instruments`` (drivers and fakes), ``protocols`` (E2E, CAN, telemetry, UDS, APP
simulator), ``dut`` (flash adapters), ``bench`` (bench facade, lock, safe state),
``analysis``, ``reporting`` and ``pytest_plugin``. ``gen`` is generated from ``interfaces/``.
"""

from importlib.metadata import PackageNotFoundError, version

try:
    __version__ = version("locksys-hil")
except PackageNotFoundError:  # pragma: no cover - source tree without installation
    __version__ = "0+unknown"

__all__ = ["__version__"]
