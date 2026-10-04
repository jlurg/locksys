# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Unit tests for tools/ci/coverage_gate.py."""

from __future__ import annotations

import textwrap
from pathlib import Path

import pytest

from tools.ci import coverage_gate as cov

FIXTURES = Path(__file__).resolve().parent / "fixtures" / "coverage"


def _config(tmp_path: Path, body: str) -> Path:
    path = tmp_path / "gates.yaml"
    path.write_text(textwrap.dedent(body), encoding="utf-8")
    return path


LIBS_CONFIG = """
coverage:
  components:
    libs:
      exclude: ['**/test/**']
      scopes:
        - name: ls_e2e
          paths: ['libs/ls_e2e/src/**']
          line: 100
          branch: 100
          required: true
        - name: ls_common-safety
          paths: ['libs/ls_common/src/ls_{crc,evset,tmr}*.c']
          line: 100
          branch: 100
        - name: ls_common
          paths: ['libs/ls_common/src/**']
          line: 95
          branch: 90
"""


def test_parse_gcovr_json() -> None:
    files = {item.path: item for item in cov.parse_report(FIXTURES / "gcovr.json")}
    assert files["src/ls_e2e.c"].totals("line") == (4, 4)
    assert files["src/ls_e2e.c"].totals("branch") == (2, 2)
    assert files["src/ls_e2e_cfg.c"].totals("line") == (2, 3)
    assert files["src/ls_e2e_cfg.c"].totals("branch") == (1, 2)


def test_parse_gcovr_summary() -> None:
    files = {item.path: item for item in cov.parse_report(FIXTURES / "gcovr_summary.json")}
    assert files["src/ls_ring.c"].totals("line") == (47, 50)
    assert files["src/ls_ring.c"].totals("branch") == (18, 20)


def test_parse_lcov() -> None:
    files = {item.path: item for item in cov.parse_report(FIXTURES / "app.lcov")}
    controller = files["lib/features/window/domain/hold_to_run_controller.dart"]
    assert controller.totals("line") == (3, 4)
    assert controller.totals("branch") == (1, 2)


def test_unknown_format_is_rejected(tmp_path: Path) -> None:
    report = tmp_path / "x.txt"
    report.write_text("not coverage", encoding="utf-8")
    with pytest.raises(cov.CoverageInputError):
        cov.parse_report(report)


@pytest.mark.parametrize(
    ("pattern", "path", "matches"),
    [
        ("libs/ls_e2e/src/**", "libs/ls_e2e/src/ls_e2e.c", True),
        ("libs/ls_e2e/src/**", "libs/ls_e2e/src/sub/x.c", True),
        ("libs/ls_e2e/src/**", "libs/ls_e2e/test/x.c", False),
        ("**/test/**", "libs/ls_e2e/test/test_x.c", True),
        ("**/test/**", "test/x.c", True),
        ("libs/ls_common/src/ls_{crc,evset,tmr}*.c", "libs/ls_common/src/ls_crc8.c", True),
        ("libs/ls_common/src/ls_{crc,evset,tmr}*.c", "libs/ls_common/src/ls_ring.c", False),
        ("app/lib/**/domain/**", "app/lib/features/window/domain/a.dart", True),
        ("firmware/dcu/src/*.c", "firmware/dcu/src/app/a.c", False),
    ],
)
def test_glob_to_regex(pattern: str, path: str, matches: bool) -> None:
    assert (cov.glob_to_regex(pattern).match(path) is not None) is matches


@pytest.mark.parametrize(
    ("raw", "strip", "prefix", "expected"),
    [
        ("src/ls_e2e.c", [], "libs/ls_e2e", "libs/ls_e2e/src/ls_e2e.c"),
        ("/project/libs/ls_e2e/src/ls_e2e.c", ["/project/"], "", "libs/ls_e2e/src/ls_e2e.c"),
        ("/repo/libs/x.c", [], "", "libs/x.c"),
        ("/elsewhere/x.c", [], "", "/elsewhere/x.c"),
        ("../src/a.c", [], "firmware/dcu/test", "firmware/dcu/src/a.c"),
    ],
)
def test_map_path(raw: str, strip: list[str], prefix: str, expected: str) -> None:
    assert cov.map_path(raw, "/repo", strip, prefix) == expected


def test_libs_gate_fails_below_threshold(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    config = _config(tmp_path, LIBS_CONFIG)
    args = [
        "--config",
        str(config),
        "--component",
        "libs",
        "--report",
        str(FIXTURES / "gcovr.json"),
    ]
    assert cov.main([*args, "--path-prefix", "libs/ls_e2e"]) == cov.EXIT_FAIL
    out = capsys.readouterr().out
    assert "ls_e2e: line coverage 85.7% (6/7) below 100%" in out
    assert "ls_e2e: branch coverage 75.0% (3/4) below 100%" in out


def test_report_only_never_fails(tmp_path: Path) -> None:
    config = _config(tmp_path, LIBS_CONFIG)
    args = [
        "--config",
        str(config),
        "--component",
        "libs",
        "--report",
        str(FIXTURES / "gcovr.json"),
    ]
    assert cov.main([*args, "--path-prefix", "libs/ls_e2e", "--report-only"]) == cov.EXIT_PASS


def test_first_matching_scope_and_required_scope(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    config = _config(tmp_path, LIBS_CONFIG)
    args = [
        "--config",
        str(config),
        "--component",
        "libs",
        "--report",
        str(FIXTURES / "gcovr_summary.json"),
    ]
    assert cov.main([*args, "--path-prefix", "libs/ls_common"]) == cov.EXIT_FAIL
    out = capsys.readouterr().out
    assert "ls_e2e: no coverage data for a required scope" in out
    assert "ls_common: line coverage 94.0% (47/50) below 95%" in out
    assert "ls_common-safety" not in out.split("error:", 1)[1]


def test_threshold_boundary_is_exact() -> None:
    scope_fraction = cov._as_fraction(66.7, "t")
    assert not cov._meets(2, 3, scope_fraction)
    assert cov._meets(2, 3, cov._as_fraction(66.6, "t"))
    assert cov._meets(0, 0, cov._as_fraction(100, "t"))


def test_per_file_scope(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    config = _config(
        tmp_path,
        """
        coverage:
          components:
            libs:
              scopes:
                - name: all
                  paths: ['libs/**']
                  line: 80
                  per_file: true
        """,
    )
    args = [
        "--config",
        str(config),
        "--component",
        "libs",
        "--report",
        str(FIXTURES / "gcovr.json"),
    ]
    assert cov.main([*args, "--path-prefix", "libs/ls_e2e"]) == cov.EXIT_FAIL
    out = capsys.readouterr().out
    assert "all (libs/ls_e2e/src/ls_e2e_cfg.c): line coverage 66.7% (2/3) below 80%" in out
    assert "all (libs/ls_e2e/test/test_ls_e2e.c)" in out


def test_no_scoped_data_fails(tmp_path: Path) -> None:
    config = _config(tmp_path, LIBS_CONFIG)
    args = [
        "--config",
        str(config),
        "--component",
        "libs",
        "--report",
        str(FIXTURES / "gcovr.json"),
    ]
    assert cov.main([*args, "--path-prefix", "elsewhere"]) == cov.EXIT_FAIL


def test_merge_of_line_level_reports(tmp_path: Path) -> None:
    first = tmp_path / "a.info"
    second = tmp_path / "b.info"
    first.write_text("SF:src/x.c\nDA:1,1\nDA:2,0\nend_of_record\n", encoding="utf-8")
    second.write_text("SF:src/x.c\nDA:1,0\nDA:2,3\nend_of_record\n", encoding="utf-8")
    merged = cov.parse_report(first)[0]
    merged.merge(cov.parse_report(second)[0])
    assert merged.totals("line") == (2, 2)


def test_ceedling_project_resolution(tmp_path: Path) -> None:
    project = tmp_path / "libs" / "ls_e2e"
    project.mkdir(parents=True)
    (project / "project.yml").write_text(":project:\n  :build_root: out\n", encoding="utf-8")
    report, prefix = cov.ceedling_report(project, tmp_path)
    assert report == project / "out" / "artifacts" / "gcov" / "gcovr" / "GcovCoverage.json"
    assert prefix == "libs/ls_e2e"
    nested = tmp_path / "firmware" / "dcu" / "test"
    nested.mkdir(parents=True)
    (nested / "project.yml").write_text(
        ":gcov:\n  :gcovr:\n    :report_root: '..'\n    :json_artifact_filename: cov.json\n",
        encoding="utf-8",
    )
    report, prefix = cov.ceedling_report(nested, tmp_path)
    assert report.name == "cov.json"
    assert prefix == "firmware/dcu"


def test_ceedling_project_cli(tmp_path: Path) -> None:
    project = tmp_path / "libs" / "ls_e2e"
    artifacts = project / "build" / "artifacts" / "gcov" / "gcovr"
    artifacts.mkdir(parents=True)
    (project / "project.yml").write_text(
        ":project:\n  :use_test_preprocessor: :all\n", encoding="utf-8"
    )
    (artifacts / "GcovCoverage.json").write_text(
        (FIXTURES / "gcovr.json").read_text(encoding="utf-8"), encoding="utf-8"
    )
    config = _config(tmp_path, LIBS_CONFIG)
    args = ["--config", str(config), "--component", "libs", "--root", str(tmp_path)]
    assert cov.main([*args, "--ceedling-project", str(project), "--report-only"]) == cov.EXIT_PASS


def test_missing_component_is_an_input_error(tmp_path: Path) -> None:
    config = _config(tmp_path, LIBS_CONFIG)
    args = [
        "--config",
        str(config),
        "--component",
        "nope",
        "--report",
        str(FIXTURES / "gcovr.json"),
    ]
    assert cov.main(args) == cov.EXIT_INPUT_ERROR


@pytest.mark.parametrize("component", ["libs", "dcu", "cgw", "app", "python"])
def test_repository_configuration_is_valid(component: str) -> None:
    loaded = cov.load_component(cov.DEFAULT_CONFIG, component)
    assert loaded.scopes


def test_app_component_is_report_only(capsys: pytest.CaptureFixture[str]) -> None:
    args = ["--component", "app", "--report", str(FIXTURES / "app.lcov"), "--path-prefix", "app"]
    assert cov.main(args) == cov.EXIT_PASS
    out = capsys.readouterr().out
    assert "warning: domain: line coverage 75.0% (3/4) below 95%" in out
    assert "app_localizations" not in out


def test_prefixed_reports_merge_app_and_packages(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    package_report = tmp_path / "lcov.info"
    package_report.write_text(
        "SF:lib/src/frame.dart\nDA:1,1\nDA:2,0\nLF:2\nLH:1\nend_of_record\n", encoding="utf-8"
    )
    args = [
        "--component",
        "app",
        "--prefixed-report",
        "app",
        str(FIXTURES / "app.lcov"),
        "--prefixed-report",
        "app/packages/locksys_protocol",
        str(package_report),
    ]
    assert cov.main(args) == cov.EXIT_PASS
    out = capsys.readouterr().out
    assert "warning: locksys_protocol: line coverage 50.0% (1/2) below 95%" in out
    assert "warning: domain: line coverage 75.0% (3/4) below 95%" in out
