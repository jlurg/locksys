# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Bench lock and bench facade with fake instruments."""

from pathlib import Path

import pytest

from locksys_hil.bench import Bench, BenchLock
from locksys_hil.config.models import BenchConfig, PsuConfig, StimulusConfig
from locksys_hil.errors import BenchBusyError, BenchInfrastructureError
from locksys_hil.instruments.fakes import FakePsu, FakeStimulus


def _config(tmp_path: Path) -> BenchConfig:
    return BenchConfig(
        name="unit",
        lock_file=str(tmp_path / "lock" / "bench.lock"),
        capabilities=["psu.kl30", "stimulus"],
        psu=PsuConfig(driver="fake", resource="fake"),
        stimulus=StimulusConfig(driver="fake"),
    )


def test_lock_is_exclusive(tmp_path: Path) -> None:
    path = tmp_path / "bench.lock"
    with BenchLock(path) as first:
        assert first.is_locked
        assert "pid=" in (first.owner() or "")
        with pytest.raises(BenchBusyError, match="BENCH_BUSY"):
            BenchLock(path).acquire()
    assert first.owner() is None
    BenchLock(path).acquire()


def test_bench_open_safe_state_and_close(tmp_path: Path) -> None:
    bench = Bench.open(_config(tmp_path))
    assert isinstance(bench.psu, FakePsu)
    assert isinstance(bench.stimulus, FakeStimulus)
    assert bench.missing(["dcu", "psu.kl30"]) == ["dcu"]
    bench.psu.set_output(1, True)
    bench.stimulus.write("MODE SIM")
    calls: list[str] = []
    bench.add_safe_state_hook(lambda: calls.append("hook"))
    bench.safe_state()
    assert not bench.psu.output_enabled(1)
    assert bench.stimulus.query("MODE?") == "OFF"
    assert calls == ["hook"]
    bench.close()
    bench.close()
    assert not bench.lock.is_locked


def test_safe_state_continues_after_a_failing_step(tmp_path: Path) -> None:
    bench = Bench.open(_config(tmp_path))
    assert bench.psu is not None

    def broken() -> None:
        raise RuntimeError("hook failure")

    bench.add_safe_state_hook(broken)
    bench.psu.set_output(1, True)
    bench.safe_state()
    assert not bench.psu.output_enabled(1)
    bench.close()


def test_self_test_rejects_supply_on_in_sim(tmp_path: Path) -> None:
    bench = Bench.open(_config(tmp_path))
    assert bench.psu is not None
    bench.psu.set_output(1, True)
    with pytest.raises(BenchInfrastructureError, match="actuator supply"):
        bench.self_test()
    bench.close()


def test_stimulus_major_version_checked(tmp_path: Path) -> None:
    bench = Bench.open(_config(tmp_path))
    assert isinstance(bench.stimulus, FakeStimulus)
    bench.stimulus.version = "2.0"
    with pytest.raises(BenchInfrastructureError, match="protocol major"):
        bench.self_test()
    bench.close()
