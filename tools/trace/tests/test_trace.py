# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Unit tests for tools/trace/trace.py on a synthetic repository."""

from __future__ import annotations

import json
import textwrap
from pathlib import Path

import pytest

from tools.trace import trace

REPO_ROOT = Path(__file__).resolve().parents[3]

SRS = """
    # Requirements

    | ID | Requirement | Acceptance criterion | Alloc | Ver | Stage | Tag | Parents |
    |---|---|---|---|---|---|---|---|
    | SYS-001 | Shown state. | 10 runs. | A | HIL, UT | A | [SAF] | STK-001 |
    | SYS-002 | Later feature. | — | D | HIL | B | — | STK-001 |
    | SYS-003 | Unsatisfied. | — | D | IT | A | — | STK-001 |

    | ID | Requirement | Verification | Parents |
    |---|---|---|---|
    | HWR-001 | Bench supply. | R | SYS-003 |

    | STK | SYS |
    |---|---|
    | STK-001 | SYS-001, SYS-002 |

    ```text
    | SYS-999 | inside a code block, never parsed |
    ```
"""

STK = """
    | ID | Requirement |
    |---|---|
    | STK-001 | The user sees the state. |
"""

SWR = """
    | ID | Requirement | Parents | Ver | Stage | Tag | MS |
    |---|---|---|---|---|---|---|
    | SWR-DCU-001 | Done. | SYS-001; SM-01 | UT | A | [SAF] | M2 |
    | SWR-DCU-002 | Missing tags. | SYS-001 | UT | A | — | M2 |
    | SWR-DCU-120 | Stage B. | SYS-002 | UT | B | — | LATER |
"""

SAFETY = """
    | ID | Mechanism | Alloc | Goals | Requirements | Stage | Verification |
    |---|---|---|---|---|---|---|
    | SM-01 | Supervision. | DCU | SG-01 | SWR-DCU-001 | A | TST-HIL-SYS-001 |
    | SM-02 | Orphan. | DCU | SG-01 | — | A | — |

    | ID | Rating | Safety goal |
    |---|---|---|
    | SG-01 | B | No unintended motion. |
"""

SECURITY = """
    | ID | Requirement | Alloc | Goals | Parent | Verification |
    |---|---|---|---|---|---|
    | CSR-001 | Reviewed by procedure. | CGW | — | SYS-001 | M (TST-MAN-SYS-001) |
    | CSR-002 | Unverified. | CGW | — | SYS-001 | R |
"""

CATALOGUE = """
    | ID | Title | Verifies | Topology |
    |---|---|---|---|
    | TST-HIL-SYS-001 | Shown state | SYS-001 | T3 |

    | ID | Title | Verifies or supports | Milestone |
    |---|---|---|---|
    | TST-MAN-SYS-001 | Bus resistance | bench | M1 |
"""

C_SOURCE = """
    /* @satisfies SWR-DCU-001 */
    void Win_Main(void) {}
    /* @satisfies SWR-DCU-777 */
"""

C_TEST = """
    /* @verifies SWR-DCU-001, SM-01 */
    void test_win(void) {}
    /* @verifies SYS-37 */
"""

HIL_TEST = """
    import pytest


    @pytest.mark.test_id("TST-HIL-SYS-001")
    @pytest.mark.verifies(
        "SYS-001",
    )
    def test_state() -> None:
        pass
"""


def _write(root: Path, rel: str, text: str) -> None:
    path = root / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(textwrap.dedent(text), encoding="utf-8")


@pytest.fixture
def repo(tmp_path: Path) -> Path:
    _write(tmp_path, "docs/02_system/system_requirements.md", SRS)
    _write(tmp_path, "docs/01_stakeholder/stakeholder_requirements.md", STK)
    _write(tmp_path, "docs/04_software/dcu/software_requirements.md", SWR)
    _write(tmp_path, "docs/05_safety/safety_concept.md", SAFETY)
    _write(tmp_path, "docs/06_security/security_concept.md", SECURITY)
    _write(tmp_path, "docs/07_verification/hil_test_catalog.md", CATALOGUE)
    _write(
        tmp_path, "docs/08_process/coding_standard.md", "Example: `/* @satisfies SWR-DCU-999 */`\n"
    )
    _write(tmp_path, "firmware/dcu/src/app/win.c", C_SOURCE)
    _write(tmp_path, "firmware/dcu/test/test_win.c", C_TEST)
    _write(tmp_path, "firmware/dcu/gen/generated.c", "/* @satisfies SWR-DCU-888 */\n")
    _write(tmp_path, "hil/tests/t3_system/test_state.py", HIL_TEST)
    _write(tmp_path, "tools/trace/tests/test_example.py", "# @verifies SWR-DCU-555\n")
    return tmp_path


def _violations(repo: Path, release: bool = False) -> set[tuple[str, str, str]]:
    matrix = trace.build(repo, [])
    return {(v.rule, v.id, v.message) for v in trace.evaluate(matrix, release=release)}


def test_definitions_and_attributes(repo: Path) -> None:
    items = trace.parse_docs(repo / "docs", repo)
    assert "SYS-999" not in items
    assert set(items) >= {"SYS-001", "HWR-001", "SWR-DCU-001", "SM-01", "SG-01", "CSR-001"}
    sys1 = items["SYS-001"]
    assert sys1.is_mvp
    assert sys1.is_safety
    assert sys1.methods() == {"HIL", "UT"}
    assert sys1.parents == {"STK-001"}
    assert not items["SYS-002"].is_mvp
    assert not items["SWR-DCU-120"].is_mvp
    assert items["SM-01"].implemented_by == {"SWR-DCU-001"}
    assert items["TST-HIL-SYS-001"].verifies == {"SYS-001"}


def test_tags_are_read_from_sources_only(repo: Path) -> None:
    tags, problems = trace.scan_tags(repo)
    found = {(t.relation, t.id, t.level) for t in tags}
    assert ("satisfies", "SWR-DCU-001", "impl") in found
    assert ("verifies", "SWR-DCU-001", "UT") in found
    assert ("verifies", "SM-01", "UT") in found
    assert ("verifies", "SYS-001", "HIL") in found
    assert ("test_id", "TST-HIL-SYS-001", "HIL") in found
    ids = {t.id for t in tags}
    assert not ids & {"SWR-DCU-999", "SWR-DCU-888", "SWR-DCU-555"}
    assert [p.id for p in problems] == ["SYS-37"]
    hil = next(t for t in tags if t.id == "SYS-001")
    assert (hil.path, hil.line) == ("hil/tests/t3_system/test_state.py", 6)


def test_rules(repo: Path) -> None:
    violations = _violations(repo)
    assert ("T1", "SWR-DCU-777", "unknown identifier at firmware/dcu/src/app/win.c:4") in violations
    assert any(v[:2] == ("T1", "SYS-37") for v in violations)
    assert ("T2", "SYS-003", "no SWR or HWR satisfies it") not in violations
    assert ("T3:DCU", "SWR-DCU-002", "no @satisfies location") in violations
    assert ("T3:DCU", "SWR-DCU-002", "no @verifies test") in violations
    assert not any(v[1] in {"SWR-DCU-001", "SWR-DCU-120", "SYS-001", "SM-01"} for v in violations)
    assert ("T5", "SM-02", "no implementing requirement") in violations
    assert ("T6", "CSR-002", "no verification entry") in violations
    assert not any(v[1] == "CSR-001" for v in violations)
    assert not any(v[0] == "T7" for v in violations)


def test_t2_and_t4(repo: Path) -> None:
    _write(repo, "hil/tests/t3_system/test_state.py", "")
    _write(
        repo,
        "docs/02_system/extra.md",
        """
        | ID | Requirement | Acceptance criterion | Alloc | Ver | Stage | Tag | Parents |
        |---|---|---|---|---|---|---|---|
        | SYS-004 | Orphan. | — | D | UT | A | — | STK-001 |
        """,
    )
    violations = _violations(repo)
    assert ("T2", "SYS-004", "no SWR or HWR satisfies it") in violations
    assert ("T4", "SYS-001", "no HIL test") in violations


def test_release_rule_uses_junit(repo: Path, tmp_path: Path) -> None:
    junit = tmp_path / "junit.xml"
    junit.write_text(
        textwrap.dedent(
            """\
            <testsuites><testsuite>
              <testcase classname="hil" name="ok">
                <properties>
                  <property name="test_id" value="TST-HIL-SYS-001"/>
                  <property name="verifies" value="SYS-001"/>
                </properties>
              </testcase>
              <testcase classname="unit" name="bad">
                <properties><property name="verifies" value="SWR-DCU-001"/></properties>
                <failure message="x"/>
              </testcase>
            </testsuite></testsuites>
            """
        ),
        encoding="utf-8",
    )
    matrix = trace.build(repo, [junit])
    violations = {(v.rule, v.id) for v in trace.evaluate(matrix, release=True)}
    assert ("T7", "SYS-001") not in violations
    assert ("T7", "SWR-DCU-001") in violations
    assert ("T7", "SYS-003") in violations


def test_phases() -> None:
    sample = [trace.Violation("T3:CGW", "SWR-CGW-001", "x"), trace.Violation("T1", "SYS-1", "y")]
    assert trace.blocking(sample, 1, release=False) == []
    assert [v.rule for v in trace.blocking(sample, 2, release=False)] == ["T1"]
    assert len(trace.blocking(sample, 3, release=False)) == 2


def test_report_and_gate_outputs(
    repo: Path, tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    out = tmp_path / "out"
    summary = tmp_path / "summary.md"
    monkeypatch.setenv("GITHUB_STEP_SUMMARY", str(summary))
    assert trace.main(["--report", "--root", str(repo), "--out-dir", str(out)]) == 0
    data = json.loads((out / "trace_matrix.json").read_text(encoding="utf-8"))
    assert {row["id"] for row in data["items"]} >= {"SYS-001", "SWR-DCU-001"}
    assert (out / "trace_matrix.csv").read_text(encoding="utf-8").startswith("id,kind,mvp")
    assert "| SYS-001 | yes |" in (out / "trace_matrix.md").read_text(encoding="utf-8")
    assert "## Traceability" in summary.read_text(encoding="utf-8")
    assert trace.main(["--gate", "--phase", "1", "--root", str(repo), "--out-dir", str(out)]) == 0
    assert trace.main(["--gate", "--phase", "2", "--root", str(repo), "--out-dir", str(out)]) == 1


def test_report_never_fails_on_bad_input(tmp_path: Path) -> None:
    junit = tmp_path / "broken.xml"
    junit.write_text("<testsuite", encoding="utf-8")
    args = ["--root", str(tmp_path), "--out-dir", str(tmp_path / "o"), "--junit", str(junit)]
    assert trace.main(["--report", *args]) == 0
    assert trace.main(["--gate", *args]) == 2


def test_repository_report_exits_zero(tmp_path: Path) -> None:
    assert trace.main(["--report", "--root", str(REPO_ROOT), "--out-dir", str(tmp_path)]) == 0
    data = json.loads((tmp_path / "trace_matrix.json").read_text(encoding="utf-8"))
    assert not [v for v in data["violations"] if v["rule"] == "T1"]
