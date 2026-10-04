# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Location of the repository interface artefacts used by the framework."""

import os
from functools import cache
from pathlib import Path

from locksys_hil.errors import ConfigError

REPO_ROOT_ENV = "LOCKSYS_REPO_ROOT"
"""Environment variable that overrides the repository root discovery."""

_MARKER = Path("interfaces") / "can" / "locksys.dbc"


@cache
def repo_root() -> Path:
    """Return the repository root (the directory that contains ``interfaces/can/locksys.dbc``).

    The root is taken from ``LOCKSYS_REPO_ROOT`` when set, otherwise it is searched upwards from
    the current directory and from this file.

    Raises:
        ConfigError: no repository root was found.
    """
    override = os.environ.get(REPO_ROOT_ENV)
    if override:
        root = Path(override).resolve()
        if not (root / _MARKER).is_file():
            raise ConfigError(f"{REPO_ROOT_ENV}={override} does not contain {_MARKER}")
        return root
    for start in (Path.cwd(), Path(__file__).resolve()):
        for candidate in (start, *start.parents):
            if (candidate / _MARKER).is_file():
                return candidate
    raise ConfigError(f"repository root not found (no {_MARKER}); set {REPO_ROOT_ENV}")


def interfaces_dir() -> Path:
    """Return ``interfaces/`` of the repository."""
    return repo_root() / "interfaces"


def vectors_file(name: str) -> Path:
    """Return the path of a shared test-vector file, e.g. ``e2e_v1.json``."""
    return interfaces_dir() / "vectors" / name


def dbc_file() -> Path:
    """Return the normative CAN database ``interfaces/can/locksys.dbc``."""
    return repo_root() / _MARKER
