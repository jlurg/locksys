# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Exception hierarchy of the HIL framework.

Infrastructure errors are reported as ERROR, never as FAIL, and end the pytest session with
exit code 10 (LS-HIL-001 section 9.5).
"""

INFRASTRUCTURE_EXIT_CODE = 10
"""pytest session exit status after any infrastructure error."""


class HilError(Exception):
    """Base class of all framework errors."""


class ConfigError(HilError):
    """Invalid or missing bench configuration."""


class BenchInfrastructureError(HilError):
    """Bench fault that is not a product failure (instrument time-out, enumeration, self-test)."""


class BenchBusyError(BenchInfrastructureError):
    """The bench lock is held by another session (``BENCH_BUSY``)."""


class ProtocolError(HilError):
    """Malformed data received from a device under test."""
