# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Unit tests for tools/ci/cstat_gate.py."""

from __future__ import annotations

import json
import re
from pathlib import Path

import pytest

from tools.ci import cstat_gate as cg

FIXTURES = Path(__file__).resolve().parent / "fixtures" / "cstat"
REPO_ROOT = Path(__file__).resolve().parents[3]
LAB_ROOT = "C:/labhost/runners/iar/_work/locksys/locksys"


def _source(root: Path, relative: str, text: str) -> Path:
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


def _suppression_check(root: Path) -> tuple[cg.CheckResult, cg.CheckResult]:
    settings = cg.load_settings(None)
    records = cg.load_deviations(FIXTURES / "deviations.yaml")
    pattern = re.compile(settings["deviation_id_pattern"])
    suppressions, errors = cg.scan_sources(
        [root / "src"], settings["source_extensions"], pattern, root
    )
    return cg.check_suppressions(suppressions, errors, records, settings)


# --------------------------------------------------------------------------- SARIF


def test_sarif_open_and_suppressed_results() -> None:
    document = cg.read_sarif(FIXTURES / "findings.sarif")
    findings = cg.collect_findings([document], LAB_ROOT)
    assert [(f.rule, f.path, f.line, f.suppressed) for f in findings] == [
        ("MISRAC2012-Rule-10.4_a", "firmware/dcu/src/app/win_ctrl/win_ctrl.c", 42, False),
        ("MISRAC2012-Rule-11.4", "firmware/dcu/src/mcal/gpio/gpio.c", 17, True),
        ("MISRAC2012-Rule-8.4", "firmware/dcu/src/main.c", 5, False),
        ("MISRAC2012-Rule-8.4", "libs/ls_e2e/src/ls_e2e.c", 88, False),
    ]
    result = cg.check_sarif(findings, 1)
    assert not result.passed
    assert len(result.errors) == 3


def test_clean_sarif_passes() -> None:
    document = cg.read_sarif(FIXTURES / "clean.sarif")
    result = cg.check_sarif(cg.collect_findings([document], LAB_ROOT), 1)
    assert result.passed
    assert "0 open, 1 suppressed" in result.notes[0]


def test_missing_sarif_output_fails() -> None:
    assert not cg.check_sarif([], 0).passed


@pytest.mark.parametrize(
    ("suppressions", "expected"),
    [
        (None, False),
        ([], False),
        ([{"kind": "inSource"}], True),
        ([{"kind": "inSource", "status": "accepted"}], True),
        ([{"kind": "external", "status": "underReview"}], False),
        ([{"kind": "inSource"}, {"kind": "external", "status": "rejected"}], False),
    ],
)
def test_suppression_semantics(suppressions: object, expected: bool) -> None:
    result: dict[str, object] = {"ruleId": "X"}
    if suppressions is not None:
        result["suppressions"] = suppressions
    assert cg.is_suppressed(result) is expected


@pytest.mark.parametrize(
    ("uri", "base", "expected"),
    [
        (
            "file:///C:/labhost/runners/iar/_work/locksys/locksys/firmware/dcu/src/a.c",
            None,
            "firmware/dcu/src/a.c",
        ),
        ("file:///c:/LABHOST/runners/iar/_work/locksys/locksys/libs/x.c", None, "libs/x.c"),
        ("C:\\labhost\\runners\\iar\\_work\\locksys\\locksys\\libs\\y.c", None, "libs/y.c"),
        ("./firmware/dcu/src/b.c", None, "firmware/dcu/src/b.c"),
        (
            "src/c.c",
            "file:///C:/labhost/runners/iar/_work/locksys/locksys/firmware/dcu/",
            "firmware/dcu/src/c.c",
        ),
        ("file:///C:/iar/arm/inc/c/stdint.h", None, "C:/iar/arm/inc/c/stdint.h"),
        ("file:///C:/labhost/runners/iar/_work/locksys/locksys/a%20b.c", None, "a b.c"),
    ],
)
def test_relativize(uri: str, base: str | None, expected: str) -> None:
    assert cg.relativize(uri, LAB_ROOT, base) == expected


def test_merge_sarif_single_run_with_relative_uris(tmp_path: Path) -> None:
    out = tmp_path / "merged.sarif"
    args = [
        "--sarif",
        str(FIXTURES / "findings.sarif"),
        str(FIXTURES / "clean.sarif"),
        "--source-root",
        LAB_ROOT,
        "--sarif-out",
        str(out),
    ]
    assert cg.main(args) == cg.EXIT_FAIL
    merged = json.loads(out.read_text(encoding="utf-8"))
    assert merged["version"] == "2.1.0"
    assert len(merged["runs"]) == 1
    run = merged["runs"][0]
    rule_ids = [rule["id"] for rule in run["tool"]["driver"]["rules"]]
    assert rule_ids == ["MISRAC2012-Rule-10.4_a", "MISRAC2012-Rule-11.4", "MISRAC2012-Rule-8.4"]
    for result in run["results"]:
        assert rule_ids[result["ruleIndex"]] == result["ruleId"]
        for location in result["locations"]:
            artifact = location["physicalLocation"]["artifactLocation"]
            assert not artifact["uri"].startswith(("file:", "C:"))
            assert "uriBaseId" not in artifact
            assert "index" not in artifact
    assert len(run["results"]) == 6


def test_unreadable_sarif_is_an_input_error(tmp_path: Path) -> None:
    bad = tmp_path / "bad.sarif"
    bad.write_text("{not json", encoding="utf-8")
    assert cg.main(["--sarif", str(bad)]) == cg.EXIT_INPUT_ERROR
    assert cg.main(["--sarif", str(tmp_path / "missing.sarif")]) == cg.EXIT_INPUT_ERROR


def test_sarif_directory_without_files_fails(tmp_path: Path) -> None:
    assert cg.main(["--sarif", str(tmp_path)]) == cg.EXIT_FAIL


# --------------------------------------------------------------------------- suppressions


def test_justified_suppressions_pass(tmp_path: Path) -> None:
    _source(
        tmp_path,
        "src/mcal/gpio.c",
        "/*cstat !MISRAC2012-Rule-11.4 : DEV-DCU-001 CMSIS register access */\n"
        "x = (uint32_t)GPIOA;\n"
        "//cstat -MISRAC2012-Rule-10.4_a : DEV-DCU-002 essential type of the CMSIS macro\n"
        "y = z;\n"
        "//cstat +MISRAC2012-Rule-10.4_a\n"
        "//cstat : continuation of a justification\n",
    )
    justified, referenced = _suppression_check(tmp_path)
    assert justified.passed, justified.errors
    assert referenced.passed, referenced.errors


@pytest.mark.parametrize(
    ("text", "message"),
    [
        ("/*cstat !MISRAC2012-Rule-11.4 */\n", "no deviation id"),
        ("/*cstat !MISRAC2012-Rule-11.4 : DEV-DCU-099 x */\n", "unknown deviation DEV-DCU-099"),
        ("/*cstat !MISRAC2012-Rule-15.5 : DEV-DCU-003 x */\n", "has status 'proposed'"),
        ("/*cstat !MISRAC2012-Rule-8.4 : DEV-DCU-001 x */\n", "does not cover MISRAC2012-Rule-8.4"),
        ("/*cstat #MISRAC2012-Rule-* : DEV-DCU-001 x */\n", "does not cover MISRAC2012-Rule-*"),
        ("/*cstat MISRAC2012-Rule-11.4 : DEV-DCU-001 */\n", "malformed C-STAT directive"),
        ('#pragma cstat_disable="MISRAC2012-Rule-11.4"\n', "no deviation id"),
        ("#pragma cstat_suppress\n", "without a quoted check tag"),
    ],
)
def test_invalid_suppressions_fail(tmp_path: Path, text: str, message: str) -> None:
    _source(
        tmp_path, "src/mcal/gpio.c", "/*cstat !MISRAC2012-Rule-11.4 : DEV-DCU-001 ok */\n\n" + text
    )
    justified, _ = _suppression_check(tmp_path)
    assert not justified.passed
    assert any(message in error for error in justified.errors), justified.errors


def test_pragma_with_deviation_comment_on_previous_line(tmp_path: Path) -> None:
    _source(
        tmp_path,
        "src/mcal/gpio.c",
        "/* DEV-DCU-001: register access */\n"
        '#pragma cstat_disable="MISRAC2012-Rule-11.4", \\\n'
        '                      "MISRAC2012-Rule-10.4_b"\n'
        "x = (uint32_t)GPIOA;\n"
        '#pragma cstat_restore="MISRAC2012-Rule-11.4"\n',
    )
    justified, _ = _suppression_check(tmp_path)
    assert any("does not cover MISRAC2012-Rule-10.4_b" in error for error in justified.errors)
    assert not any("MISRAC2012-Rule-11.4" in error for error in justified.errors)


def test_suppression_in_generated_code_fails(tmp_path: Path) -> None:
    _source(
        tmp_path,
        "src/gen_vs/release/WinCtrl.c",
        "/*cstat !MISRAC2012-Rule-11.4 : DEV-DCU-001 x */\n",
    )
    justified, _ = _suppression_check(tmp_path)
    assert any("generated code" in error for error in justified.errors)


def test_unreferenced_approved_deviation_fails(tmp_path: Path) -> None:
    _source(tmp_path, "src/app/a.c", "int a;\n")
    _, referenced = _suppression_check(tmp_path)
    assert sorted(error.split(":")[0] for error in referenced.errors) == [
        "DEV-DCU-001",
        "DEV-DCU-002",
    ]


def test_prose_comments_are_not_directives(tmp_path: Path) -> None:
    _source(
        tmp_path,
        "src/app/a.c",
        '/* C-STAT findings are reviewed weekly. */\n/** cstatistics helper */\nconst char *u = "http://x";\n',
    )
    settings = cg.load_settings(None)
    pattern = re.compile(settings["deviation_id_pattern"])
    suppressions, errors = cg.scan_sources([tmp_path / "src"], [".c"], pattern, tmp_path)
    assert suppressions == []
    assert errors == []


def test_registry_formats(tmp_path: Path) -> None:
    listed = tmp_path / "list.yaml"
    listed.write_text("- {id: DEV-LIB-001, status: Approved, cstat_tag: X}\n", encoding="utf-8")
    assert cg.load_deviations(listed)["DEV-LIB-001"].status == "approved"
    empty = tmp_path / "empty.yaml"
    empty.write_text("schema_version: 1\ndeviations: []\n", encoding="utf-8")
    assert cg.load_deviations(empty) == {}
    duplicate = tmp_path / "dup.yaml"
    duplicate.write_text("deviations:\n  - {id: DP-01}\n  - {id: DP-01}\n", encoding="utf-8")
    with pytest.raises(cg.GateInputError, match="duplicate"):
        cg.load_deviations(duplicate)
    wrong = tmp_path / "wrong.yaml"
    wrong.write_text("schema_version: 1\n", encoding="utf-8")
    with pytest.raises(cg.GateInputError):
        cg.load_deviations(wrong)


def test_repository_registry_loads() -> None:
    registry = REPO_ROOT / "docs" / "08_process" / "misra" / "deviations.yaml"
    if not registry.is_file():
        pytest.skip("deviation register not present")
    records = cg.load_deviations(registry)
    pattern = re.compile(cg.load_settings(cg.DEFAULT_CONFIG)["deviation_id_pattern"])
    assert all(pattern.fullmatch(record_id) for record_id in records)


# --------------------------------------------------------------------------- map files and compiler


def test_map_symbols_only_from_entry_list() -> None:
    symbols = cg.map_symbols((FIXTURES / "release_clean.map").read_text(encoding="utf-8"))
    assert {
        "WinCtrl_Main10ms",
        "main",
        "?main",
        "WinCtrl_SomeHelperWithAVeryLongNameThatWraps",
    } <= symbols
    assert "malloc" not in symbols


def test_release_map_with_fault_injection_and_heap_fails() -> None:
    settings = cg.load_settings(cg.DEFAULT_CONFIG)
    result = cg.check_maps([], [FIXTURES / "release_fault_injection.map"], settings)
    assert len(result.errors) == 1
    assert "FaultInj_Table, Fi_Main10ms, malloc" in result.errors[0]


def test_debug_map_allows_fault_injection_but_not_heap() -> None:
    settings = cg.load_settings(cg.DEFAULT_CONFIG)
    result = cg.check_maps([FIXTURES / "release_fault_injection.map"], [], settings)
    assert len(result.errors) == 1
    assert result.errors[0].endswith("forbidden symbol(s): malloc")
    assert cg.check_maps([], [FIXTURES / "release_clean.map"], settings).passed


@pytest.mark.parametrize(
    ("pin", "output", "passed"),
    [
        ("9.70", "IAR ANSI C/C++ Compiler V9.70.4.477/W64 for ARM", True),
        ("9.70.4", "IAR ANSI C/C++ Compiler V9.70.4.477/W64 for ARM", True),
        ("9.70.3", "IAR ANSI C/C++ Compiler V9.70.4.477/W64 for ARM", False),
        ("9.70", "IAR C/C++ Compiler V10.10.1.123 for Arm", False),
        ("9.70", "no version here", False),
        ("", "IAR ANSI C/C++ Compiler V9.70.4.477/W64 for ARM", False),
    ],
)
def test_compiler_version(pin: str, output: str, passed: bool) -> None:
    pins = {"IAR_EWARM_BASELINE": pin} if pin else {}
    assert cg.check_compiler(output, pins, "IAR_EWARM_BASELINE").passed is passed


def test_env_file_parsing() -> None:
    pins = cg.parse_env_file(FIXTURES / "versions.env")
    assert pins["IAR_EWARM_BASELINE"] == "9.70"
    assert pins["IAR_VISUAL_STATE_MIN"] == "11.2.1"
    assert pins["ARM_GCC"] == "13.3.rel1"


# --------------------------------------------------------------------------- command line


def test_full_gate_passes_on_clean_inputs(tmp_path: Path) -> None:
    _source(
        tmp_path,
        "src/mcal/gpio.c",
        "/*cstat !MISRAC2012-Rule-11.4 : DEV-DCU-001 x */\n/*cstat !MISRAC2012-Rule-10.4_a : DEV-DCU-002 y */\n",
    )
    summary = tmp_path / "summary.md"
    args = [
        "--sarif",
        str(FIXTURES / "clean.sarif"),
        "--sources",
        str(tmp_path / "src"),
        "--deviations",
        str(FIXTURES / "deviations.yaml"),
        "--release-map",
        str(FIXTURES / "release_clean.map"),
        "--compiler-version-file",
        str(FIXTURES / "iccarm_version.txt"),
        "--versions-env",
        str(FIXTURES / "versions.env"),
        "--source-root",
        str(tmp_path),
        "--summary",
        str(summary),
    ]
    assert cg.main(args) == cg.EXIT_PASS
    text = summary.read_text(encoding="utf-8")
    for code in ("CS001", "CS002", "CS003", "CS004", "CS005"):
        assert code in text


def test_sources_require_deviations(tmp_path: Path) -> None:
    assert cg.main(["--sources", str(tmp_path)]) == cg.EXIT_INPUT_ERROR


def test_no_check_requested_is_an_input_error() -> None:
    assert cg.main([]) == cg.EXIT_INPUT_ERROR


def test_repository_configuration_overrides_defaults() -> None:
    settings = cg.load_settings(cg.DEFAULT_CONFIG)
    assert settings["compiler_version_key"] == "IAR_EWARM_BASELINE"
    assert "^Fi_" in settings["forbidden_symbols"]["release"]
