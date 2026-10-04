# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Unit tests for tools/ci/complexity_gate.py."""

from __future__ import annotations

from pathlib import Path

import pytest

from tools.ci import complexity_gate as cx

FIXTURES = Path(__file__).resolve().parent / "fixtures" / "complexity"


def test_parse_lizard_csv() -> None:
    functions = cx.parse_lizard_csv((FIXTURES / "lizard.csv").read_text(encoding="utf-8"))
    deep = functions[0]
    assert deep.function == "deep"
    assert deep.file == "firmware/dcu/src/app/win_ctrl/win_ctrl.c"
    assert deep.line == 1
    assert dict(deep.values) == {"nloc": 7, "ccn": 7, "parameters": 6, "nesting": 5}


def test_parse_without_nesting_column() -> None:
    row = '8,2,22,1,8,"f@1-8@a.c","a.c","f","f( int x)",1,8\n'
    assert "nesting" not in cx.parse_lizard_csv(row)[0].values


def test_malformed_csv_is_rejected() -> None:
    with pytest.raises(cx.ComplexityInputError):
        cx.parse_lizard_csv("1,2,3\n")


def test_repository_limits() -> None:
    settings = cx.load_settings(cx.DEFAULT_CONFIG)
    functions = cx.parse_lizard_csv((FIXTURES / "lizard.csv").read_text(encoding="utf-8"))
    failures, warnings = cx.evaluate(functions, settings.limits)
    assert failures == [
        "firmware/dcu/src/app/win_ctrl/win_ctrl.c:1: deep: parameters=6 exceeds 5",
        "firmware/dcu/src/services/com/com.c:100: Huge_Run: ccn=16 exceeds 15",
    ]
    assert warnings == [
        "firmware/dcu/src/app/win_ctrl/win_ctrl.c:1: deep: nesting=5 above 4",
        "firmware/dcu/src/services/sched/sched.c:20: Big_Main10ms: ccn=12 above 10",
        "firmware/dcu/src/services/sched/sched.c:20: Big_Main10ms: nloc=62 above 50",
        "firmware/dcu/src/services/com/com.c:100: Huge_Run: nloc=70 above 50",
    ]


def test_cli_with_csv(capsys: pytest.CaptureFixture[str]) -> None:
    assert cx.main(["--csv", str(FIXTURES / "lizard.csv")]) == cx.EXIT_FAIL
    assert "4 function(s), 2 failure(s), 4 warning(s)" in capsys.readouterr().out


def test_cli_missing_path_is_an_input_error(tmp_path: Path) -> None:
    assert cx.main([str(tmp_path / "missing")]) == cx.EXIT_INPUT_ERROR


def test_invalid_limits_are_rejected(tmp_path: Path) -> None:
    config = tmp_path / "gates.yaml"
    config.write_text("complexity:\n  limits:\n    ccn: {fail_above: -1}\n", encoding="utf-8")
    with pytest.raises(cx.ComplexityInputError):
        cx.load_settings(config)
    config.write_text("complexity:\n  limits:\n    halstead: {fail_above: 1}\n", encoding="utf-8")
    with pytest.raises(cx.ComplexityInputError):
        cx.load_settings(config)
