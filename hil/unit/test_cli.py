# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""``hil`` command-line interface."""

from pathlib import Path

import pytest

from locksys_hil.cli import main
from locksys_hil.protocols.telemetry import build_line

CONFIG = Path(__file__).resolve().parents[1] / "config"


def test_config_commands(capsys: pytest.CaptureFixture[str]) -> None:
    assert main(["config", "validate", str(CONFIG / "benches" / "mac-dev.yaml")]) == 0
    assert main(["config", "schema", "--check", str(CONFIG / "schema" / "bench.schema.json")]) == 0
    assert "valid" in capsys.readouterr().out


def test_schema_check_detects_drift(tmp_path: Path) -> None:
    stale = tmp_path / "schema.json"
    stale.write_text("{}\n", encoding="utf-8")
    assert main(["config", "schema", "--check", str(stale)]) == 1


def test_missing_bench_is_a_usage_error(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("LOCKSYS_HIL_BENCH", raising=False)
    assert main(["bench", "status"]) == 2


def test_safe_state_and_status(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    bench = tmp_path / "bench.yaml"
    bench.write_text(
        f"name: t\nlock_file: {tmp_path / 'l.lock'}\npsu: {{driver: fake, resource: fake}}\n",
        encoding="utf-8",
    )
    assert main(["--bench", str(bench), "safe-state"]) == 0
    assert main(["--bench", str(bench), "bench", "status"]) == 0
    assert capsys.readouterr().out.strip().endswith("free")


def test_telemetry_check(tmp_path: Path) -> None:
    log = tmp_path / "vcp.log"
    good = build_line("$LSRST", ["POWER_ON", "1"])
    log.write_text(good + "\r\n", encoding="ascii")
    assert main(["telemetry", "check", str(log)]) == 0
    log.write_text(good[:-1] + "0\r\n", encoding="ascii")
    assert main(["telemetry", "check", str(log)]) == 1
