# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Shared fixtures of the hardware-free framework tests."""

import json
from pathlib import Path
from typing import Any

import pytest

from locksys_hil.paths import repo_root


@pytest.fixture(scope="session")
def root() -> Path:
    """Repository root."""
    return repo_root()


def _load(name: str) -> Any:
    return json.loads((repo_root() / "interfaces" / "vectors" / name).read_text(encoding="utf-8"))


@pytest.fixture(scope="session")
def e2e_vectors() -> Any:
    """``interfaces/vectors/e2e_v1.json``."""
    return _load("e2e_v1.json")


@pytest.fixture(scope="session")
def session_vectors() -> Any:
    """``interfaces/vectors/app_session_v1.json``."""
    return _load("app_session_v1.json")


@pytest.fixture(scope="session")
def crypto_kat() -> Any:
    """``interfaces/vectors/crypto_kat.json``."""
    return _load("crypto_kat.json")
