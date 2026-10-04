# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""pytest plugin: markers, collection checks, JUnit properties, bench fixture, exit code 10.

Each case runs pytest in a subprocess on generated test files; the plugin is loaded through
its ``pytest11`` entry point exactly as in CI.
"""

import os
import subprocess
import sys
import textwrap
import xml.etree.ElementTree as ET
from pathlib import Path

import pytest

from locksys_hil.errors import INFRASTRUCTURE_EXIT_CODE


def _run(tmp_path: Path, source: str, *args: str) -> subprocess.CompletedProcess[str]:
    (tmp_path / "test_sample.py").write_text(textwrap.dedent(source), encoding="utf-8")
    env = {k: v for k, v in os.environ.items() if k != "LOCKSYS_HIL_BENCH"}
    return subprocess.run(
        [
            sys.executable,
            "-m",
            "pytest",
            "-p",
            "no:cacheprovider",
            "--strict-markers",
            "-q",
            str(tmp_path / "test_sample.py"),
            *args,
        ],
        cwd=tmp_path,
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )


CATALOGUE_TEST = """
    import pytest

    @pytest.mark.smoke
    @pytest.mark.hil_sim
    @pytest.mark.test_id("TST-HIL-SYS-010")
    @pytest.mark.verifies("SYS-037")
    def test_catalogue():
        pass
"""


def test_markers_registered_and_properties_written(tmp_path: Path) -> None:
    junit = tmp_path / "junit.xml"
    trace = tmp_path / "trace.json"
    result = _run(tmp_path, CATALOGUE_TEST, f"--junitxml={junit}", f"--hil-trace-json={trace}")
    assert result.returncode == 0, result.stdout + result.stderr
    properties = {p.get("name"): p.get("value") for p in ET.parse(junit).getroot().iter("property")}
    assert properties == {"test_id": "TST-HIL-SYS-010", "verifies": "SYS-037"}
    assert '"outcome": "passed"' in trace.read_text(encoding="utf-8")


def test_catalogue_test_without_test_id_is_rejected(tmp_path: Path) -> None:
    source = CATALOGUE_TEST.replace('    @pytest.mark.test_id("TST-HIL-SYS-010")\n', "")
    result = _run(tmp_path, source)
    assert result.returncode == pytest.ExitCode.USAGE_ERROR
    assert "needs exactly one test_id" in result.stderr


def test_unknown_and_malformed_identifiers_are_rejected(tmp_path: Path) -> None:
    source = CATALOGUE_TEST.replace('"SYS-037"', '"SYS-999", "SYS-37"')
    result = _run(tmp_path, source)
    assert result.returncode == pytest.ExitCode.USAGE_ERROR
    assert "unknown identifier SYS-999" in result.stderr
    assert "invalid identifier 'SYS-37'" in result.stderr


def test_registry_can_be_disabled(tmp_path: Path) -> None:
    source = CATALOGUE_TEST.replace('"SYS-037"', '"SYS-999"')
    assert _run(tmp_path, source, "--hil-registry=none").returncode == 0


def test_bench_fixture_skips_without_configuration(tmp_path: Path) -> None:
    source = """
        def test_needs_bench(bench):
            raise AssertionError("must not run")
    """
    result = _run(tmp_path, source, "-rs")
    assert result.returncode == 0
    assert "no bench configuration" in result.stdout


def _bench_yaml(tmp_path: Path, capabilities: str = "[psu.kl30, stimulus]") -> Path:
    path = tmp_path / "bench.yaml"
    path.write_text(
        textwrap.dedent(
            f"""\
            schema_version: 1
            name: unit
            lock_file: {tmp_path / "bench.lock"}
            capabilities: {capabilities}
            psu: {{driver: fake, resource: fake}}
            stimulus: {{driver: fake}}
            """
        ),
        encoding="utf-8",
    )
    return path


def test_bench_fixture_with_fake_bench(tmp_path: Path) -> None:
    source = """
        import pytest

        def test_bench(bench):
            assert bench.psu is not None
            assert bench.lock.is_locked

        @pytest.mark.requires("dcu")
        def test_missing_capability(bench):
            raise AssertionError("must not run")

        @pytest.mark.topology("T1")
        def test_topology(topology):
            raise AssertionError("must not run")
    """
    result = _run(tmp_path, source, f"--hil-bench={_bench_yaml(tmp_path)}", "-rs")
    assert result.returncode == 0, result.stdout + result.stderr
    assert "1 passed, 2 skipped" in result.stdout
    assert "bench lacks dcu" in result.stdout
    assert "topology T1 needs can, dcu" in result.stdout


def test_infrastructure_error_exit_code(tmp_path: Path) -> None:
    source = """
        from locksys_hil.errors import BenchInfrastructureError

        def test_instrument_timeout():
            raise BenchInfrastructureError("PSU time-out")
    """
    result = _run(tmp_path, source)
    assert result.returncode == INFRASTRUCTURE_EXIT_CODE
    assert "bench infrastructure errors" in result.stdout


def test_busy_bench_is_an_infrastructure_error(tmp_path: Path) -> None:
    from locksys_hil.bench.lock import BenchLock

    source = """
        def test_bench(bench):
            raise AssertionError("must not run")
    """
    with BenchLock(tmp_path / "bench.lock"):
        result = _run(tmp_path, source, f"--hil-bench={_bench_yaml(tmp_path)}")
    assert result.returncode == INFRASTRUCTURE_EXIT_CODE, result.stdout
    assert "BENCH_BUSY" in result.stdout
