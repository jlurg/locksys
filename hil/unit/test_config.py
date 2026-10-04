# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Bench configuration files and the committed JSON schema."""

from pathlib import Path

import pytest

from locksys_hil.config import bench_schema, load_bench_config
from locksys_hil.errors import ConfigError

CONFIG = Path(__file__).resolve().parents[1] / "config"


@pytest.mark.parametrize("name", ["lab-win-01.yaml", "mac-dev.yaml"])
def test_example_benches_validate(name: str) -> None:
    config = load_bench_config(CONFIG / "benches" / name)
    assert config.schema_version == 1


def test_lab_bench_lock_is_not_the_manual_reservation() -> None:
    config = load_bench_config(CONFIG / "benches" / "lab-win-01.yaml")
    assert config.lock_file.lower() != r"c:\labhost\bench.lock"
    assert config.psu is not None
    assert config.psu.kl30_current_limit_a <= 3.0


def test_schema_is_up_to_date() -> None:
    committed = (CONFIG / "schema" / "bench.schema.json").read_text(encoding="utf-8")
    assert committed == bench_schema(), (
        "run: hil config schema --write hil/config/schema/bench.schema.json"
    )


def test_invalid_configuration(tmp_path: Path) -> None:
    path = tmp_path / "bad.yaml"
    path.write_text("name: x\nlock_file: l\nunknown_key: 1\n", encoding="utf-8")
    with pytest.raises(ConfigError, match="unknown_key"):
        load_bench_config(path)
    with pytest.raises(ConfigError, match="cannot read"):
        load_bench_config(tmp_path / "missing.yaml")
