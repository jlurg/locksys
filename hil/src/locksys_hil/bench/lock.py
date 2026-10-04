# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Framework bench lock shared by CI jobs, manual sessions and the ``hil`` CLI.

The lock file is configured per bench (``lock_file``). It is distinct from the manual
reservation file ``bench.lock`` in the Lab Host root, which the runner account can only read
and which makes the job-started hook refuse HIL jobs.
"""

import getpass
import os
import socket
import time
from pathlib import Path
from types import TracebackType
from typing import Self

from filelock import FileLock, Timeout

from locksys_hil.errors import BenchBusyError


class BenchLock:
    """Exclusive, non-blocking bench lock with an owner record next to the lock file."""

    def __init__(self, path: Path) -> None:
        """Prepare a lock on ``path`` (parent directories are created on acquire)."""
        self.path = path
        self._lock = FileLock(str(path), timeout=0)
        self._owner = path.with_name(path.name + ".owner")

    @property
    def is_locked(self) -> bool:
        """True while this process holds the lock."""
        return bool(self._lock.is_locked)

    def owner(self) -> str | None:
        """Return the owner record of the current holder, if any."""
        try:
            return self._owner.read_text(encoding="utf-8").strip()
        except OSError:
            return None

    def acquire(self) -> None:
        """Take the lock without waiting.

        Raises:
            BenchBusyError: another session holds the lock (``BENCH_BUSY``).
        """
        self.path.parent.mkdir(parents=True, exist_ok=True)
        try:
            self._lock.acquire()
        except Timeout as exc:
            raise BenchBusyError(f"BENCH_BUSY: {self.path} held by {self.owner()}") from exc
        stamp = time.strftime("%Y-%m-%dT%H:%M:%S%z")
        self._owner.write_text(
            f"{getpass.getuser()}@{socket.gethostname()} pid={os.getpid()} since={stamp}\n",
            encoding="utf-8",
        )

    def release(self) -> None:
        """Release the lock if held."""
        if self._lock.is_locked:
            self._owner.unlink(missing_ok=True)
            self._lock.release()

    def __enter__(self) -> Self:
        """Acquire on entry."""
        self.acquire()
        return self

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc: BaseException | None,
        tb: TracebackType | None,
    ) -> None:
        """Release on exit."""
        self.release()
