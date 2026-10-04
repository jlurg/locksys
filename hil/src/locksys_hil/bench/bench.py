# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Bench facade (LS-HIL-001 section 9.1, layer 4).

``Bench.open`` takes the bench lock, creates the configured drivers and runs the pre-test
self-test; ``safe_state`` switches the actuator supply off and releases every stimulus output.
``safe_state`` is idempotent and also runs at interpreter exit.
"""

import atexit
import logging
from collections.abc import Callable, Iterable
from dataclasses import dataclass, field
from pathlib import Path

from locksys_hil.bench.lock import BenchLock
from locksys_hil.config.models import BenchConfig
from locksys_hil.errors import BenchInfrastructureError
from locksys_hil.instruments.base import Instrument, Psu, Stimulus
from locksys_hil.instruments.fakes import FakePsu, FakeStimulus

_log = logging.getLogger(__name__)


@dataclass
class Bench:
    """One opened bench session.

    Attributes:
        config: Validated bench configuration.
        lock: Framework bench lock.
        psu: Power supply driver, if configured.
        stimulus: Stimulus driver, if configured.
    """

    config: BenchConfig
    lock: BenchLock
    psu: Psu | None = None
    stimulus: Stimulus | None = None
    _closed: bool = field(default=False, init=False)
    _hooks: list[Callable[[], None]] = field(default_factory=list, init=False)

    @classmethod
    def open(cls, config: BenchConfig) -> "Bench":
        """Lock the bench, create the drivers and run the self-test.

        Raises:
            BenchBusyError: the bench is locked by another session.
            BenchInfrastructureError: a driver cannot be created or the self-test fails.
        """
        lock = BenchLock(Path(config.lock_file).expanduser())
        lock.acquire()
        bench = cls(config=config, lock=lock)
        try:
            bench.psu = _create_psu(config)
            bench.stimulus = _create_stimulus(config)
            atexit.register(bench.close)
            bench.self_test()
        except BaseException:
            bench.close()
            raise
        return bench

    @property
    def capabilities(self) -> frozenset[str]:
        """Capabilities declared by the configuration."""
        return frozenset(self.config.capabilities)

    def missing(self, required: Iterable[str]) -> list[str]:
        """Return the required capabilities that this bench lacks."""
        return sorted(set(required) - self.capabilities)

    def instruments(self) -> list[Instrument]:
        """Return all created drivers."""
        return [i for i in (self.psu, self.stimulus) if i is not None]

    def add_safe_state_hook(self, hook: Callable[[], None]) -> None:
        """Register an extra action of ``safe_state`` (e.g. restbus stop)."""
        self._hooks.append(hook)

    def self_test(self) -> None:
        """Pre-test self-test: instrument identities, supply state, stimulus protocol version.

        Raises:
            BenchInfrastructureError: any check fails.
        """
        for instrument in self.instruments():
            if not instrument.identify():
                raise BenchInfrastructureError(f"{type(instrument).__name__} gives no identity")
        if self.psu is not None and self.config.psu is not None:
            enabled = self.psu.output_enabled(self.config.psu.kl30_channel)
            if enabled and self.config.configuration == "SIM":
                raise BenchInfrastructureError("actuator supply is on in a HIL-SIM configuration")
        if self.stimulus is not None and self.config.stimulus is not None:
            major = self.stimulus.query("SYST:VERS?").split(".", 1)[0]
            if major != str(self.config.stimulus.protocol_major):
                raise BenchInfrastructureError(f"stimulus protocol major {major} not supported")

    def safe_state(self) -> None:
        """Actuator supply off, stimulus outputs released, registered hooks run.

        Every step runs even if an earlier one fails; failures are logged, not raised.
        """
        steps: list[Callable[[], None]] = [*self._hooks]
        steps += [i.safe_state for i in self.instruments()]
        for step in steps:
            try:
                step()
            except Exception:
                _log.exception("safe_state step failed")

    def close(self) -> None:
        """Bring the bench to the safe state, close the drivers and release the lock."""
        if self._closed:
            return
        self._closed = True
        self.safe_state()
        for instrument in self.instruments():
            try:
                instrument.close()
            except Exception:
                _log.exception("closing %s failed", type(instrument).__name__)
        self.lock.release()
        atexit.unregister(self.close)


def _create_psu(config: BenchConfig) -> Psu | None:
    if config.psu is None:
        return None
    if config.psu.driver == "fake":
        return FakePsu()
    from locksys_hil.instruments.scpi import ScpiPsu, ScpiTransport

    return ScpiPsu(ScpiTransport(config.psu.resource, config.psu.backend))


def _create_stimulus(config: BenchConfig) -> Stimulus | None:
    if config.stimulus is None:
        return None
    if config.stimulus.driver == "fake":
        return FakeStimulus()
    raise BenchInfrastructureError("stimulus serial driver is not available before M5")
