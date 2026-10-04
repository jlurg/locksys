# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""LockSys CGW simulator: APP protocol 1.0 server (LS-SAIC-001 section 8) with a plant model.

``core`` is a sans-I/O session engine driven by explicit millisecond timestamps, ``plant`` models
the DCU side, and ``server`` binds the engine to a websockets server.
"""
