# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Unit tests for the Visual State checks in tools/vs/."""

from __future__ import annotations

import json
import shutil
from pathlib import Path

import pytest

from tools.vs import check_vs_constants as cvc
from tools.vs import check_vs_gen as cvg
from tools.vs import check_vs_model as cvm
from tools.vs import vs_common as vc
from tools.vs import vs_manifest as vm

FIXTURES = Path(__file__).resolve().parent / "fixtures"
REPO_ROOT = Path(__file__).resolve().parents[3]

PARAMS = """
#define LS_T_WIN_BRAKE_MS                                (100u)
#define LS_T_WIN_REV_DEAD_MS                             (150u)
#define LS_T_LOCK_PULSE_GUARD_MS                         (600u)
#define LS_T_LOCK_SETTLE_MS                              (50u)
#define LS_T_DEBOUNCE_MS                                 (20u)
#define LS_T_LOCK_RETRY_PAUSE_MS                         (500u)
#define LS_T_INIT_MAX_MS                                 (200u)
#define LS_T_MODE_HEAL_MS                                (1000u)
#define LS_TEMP_MIN_CDEG                                 (-4000)
"""
ENUMS = """
#define LS_NODE_MODE_SAFE                                ((Ls_NodeModeType)4u)
#define LS_WINDOW_STATE_UNKNOWN                          ((Ls_WindowStateType)0u)
#define LS_WINDOW_STOP_REASON_NONE                       ((Ls_WindowStopReasonType)0u)
#define LS_WINDOW_STOP_REASON_DIR_MISMATCH               ((Ls_WindowStopReasonType)16u)
#define LS_DOOR_LOCK_STATE_UNKNOWN                       ((Ls_DoorLockStateType)0u)
#define LS_COMMAND_RESULT_OK                             ((Ls_CommandResultType)1u)
"""


def _repo(root: Path, model: bool = True, gen: bool = True) -> Path:
    (root / vc.PARAMS_HEADER).parent.mkdir(parents=True, exist_ok=True)
    (root / vc.PARAMS_HEADER).write_text(PARAMS, encoding="utf-8")
    (root / vc.ENUMS_HEADER).write_text(ENUMS, encoding="utf-8")
    options = root / vc.MODEL_DIR / "options"
    options.mkdir(parents=True)
    (options / "coder_release.opt").write_text("-api_type0\r\n", encoding="utf-8")
    for variant in vc.VARIANTS:
        (root / vc.GEN_DIR / variant).mkdir(parents=True)
        (root / vc.GEN_DIR / variant / "README.md").write_text("x\n", encoding="utf-8")
    if model:
        for path in (FIXTURES / "model").iterdir():
            shutil.copy(path, root / vc.MODEL_DIR / path.name)
    if gen:
        for variant in vc.VARIANTS:
            (root / vc.GEN_DIR / variant / "WinCtrl.c").write_text(
                "/* malloc( in a comment */\nint WinCtrlVSDeduct(int e) { return e; }\n",
                encoding="utf-8",
            )
    return root


# --------------------------------------------------------------------------- manifest


def test_manifest_placeholder_passes_without_model(tmp_path: Path) -> None:
    root = _repo(tmp_path, model=False, gen=False)
    assert vm.main(["--root", str(root), "--update"]) == 0
    assert json.loads((root / vc.MANIFEST).read_text())["model_present"] is False
    assert vm.main(["--root", str(root), "--check"]) == 0


def test_repository_manifest_check_passes() -> None:
    assert vm.main(["--root", str(REPO_ROOT)]) == 0


def test_manifest_missing_fails(tmp_path: Path) -> None:
    root = _repo(tmp_path, model=False, gen=False)
    assert vm.main(["--root", str(root)]) == 1


def test_generated_code_without_model_fails(tmp_path: Path) -> None:
    root = _repo(tmp_path, model=False, gen=True)
    (root / vc.MANIFEST).write_text(json.dumps(vm.placeholder()), encoding="utf-8")
    assert any("without a model" in p for p in vm.check(root, vm.placeholder()))


def test_manifest_round_trip_and_drift(tmp_path: Path) -> None:
    root = _repo(tmp_path)
    assert (
        vm.main(
            [
                "--root",
                str(root),
                "--update",
                "--vs-version",
                "11.2.1",
                "--verificator",
                "WinCtrl=0 critical",
            ]
        )
        == 0
    )
    data = json.loads((root / vc.MANIFEST).read_text())
    assert data["visual_state_version"] == "11.2.1"
    assert data["verificator"] == {"WinCtrl": "0 critical"}
    assert vm.main(["--root", str(root), "--check"]) == 0

    # CRLF and LF content hash the same.
    gen_file = root / vc.GEN_DIR / "release" / "WinCtrl.c"
    gen_file.write_bytes(gen_file.read_bytes().replace(b"\n", b"\r\n"))
    assert vm.main(["--root", str(root), "--check"]) == 0

    # Hand edit of generated code.
    gen_file.write_text("int edited;\n", encoding="utf-8")
    problems = vm.check(root, data)
    assert problems == ["generated file changed: firmware/dcu/gen_vs/release/WinCtrl.c"]

    # Model edited without regeneration.
    assert vm.main(["--root", str(root), "--update"]) == 0
    (root / vc.MODEL_DIR / "ModeMgr.vsr").write_text("<System Name='ModeMgr'/>", encoding="utf-8")
    assert vm.main(["--root", str(root), "--check"]) == 1
    kept = json.loads((root / vc.MANIFEST).read_text())
    assert kept["visual_state_version"] == "11.2.1"


def test_model_with_placeholder_manifest_fails(tmp_path: Path) -> None:
    root = _repo(tmp_path)
    assert vm.check(root, vm.placeholder()) == [
        "a model exists, but the manifest is the placeholder: run Invoke-VsGenerate.ps1"
    ]


def test_placeholder_claiming_model_fails(tmp_path: Path) -> None:
    root = _repo(tmp_path, model=False, gen=False)
    manifest = vm.placeholder() | {"model_present": True}
    assert vm.check(root, manifest) == ["manifest lists a model, but no .vsp file exists"]


def test_model_without_generated_code_fails(tmp_path: Path) -> None:
    root = _repo(tmp_path, gen=False)
    manifest = vm.update(root, None, None, {})
    assert "no generated code in gen_vs/release" in vm.check(root, manifest)


@pytest.mark.parametrize("text", ["{", '{"schema_version": 9}'])
def test_unsupported_manifest_is_input_error(tmp_path: Path, text: str) -> None:
    root = _repo(tmp_path, model=False, gen=False)
    (root / vc.MANIFEST).write_text(text, encoding="utf-8")
    assert vm.main(["--root", str(root)]) == 2


def test_bad_verificator_argument_is_input_error(tmp_path: Path) -> None:
    root = _repo(tmp_path)
    assert vm.main(["--root", str(root), "--update", "--verificator", "nosummary"]) == 2


# --------------------------------------------------------------------------- generated code


def test_check_vs_gen_clean_and_findings(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    root = _repo(tmp_path)
    assert cvg.main(["--root", str(root)]) == 0
    bad = root / vc.GEN_DIR / "release" / "Bad.c"
    bad.write_text(
        "void f(void) { void *p = malloc(4); free(p); }\n"
        "int g(int n, ...) { return va_arg(ap, int); }\n"
        "static void (*s_fn)(void);\n"
        'const char *s = "malloc(";\n',
        encoding="utf-8",
    )
    assert cvg.main(["--root", str(root)]) == 1
    out = capsys.readouterr().out
    assert "Bad.c:1: VSG001" in out
    assert "Bad.c:2: VSG002" in out
    assert "Bad.c:3: VSG003" in out
    assert "Bad.c:4" not in out


def test_check_vs_gen_without_code_passes(tmp_path: Path) -> None:
    root = _repo(tmp_path, model=False, gen=False)
    assert cvg.main(["--root", str(root)]) == 0


# --------------------------------------------------------------------------- model rules


def test_check_vs_model_fixture_is_clean(tmp_path: Path) -> None:
    root = _repo(tmp_path)
    assert cvm.check_model(root) == []
    assert cvm.main(["--root", str(root)]) == 0


def test_check_vs_model_findings(tmp_path: Path) -> None:
    root = _repo(tmp_path)
    (root / vc.MODEL_DIR / "DoorCtrl.vsr").unlink()
    (root / vc.MODEL_DIR / "ModeMgr.vsr").write_text(
        """<System Name="ModeMgr">
  <Event Name="SafeRequest"><Parameter Name="p"/></Event>
  <ActionFunction Name="doIt"/>
  <Variable Name="degraded"/>
  <Constant Name="HEAL"/>
  <Transition Name="M1" Action="aSetMode(kSafe)"/>
  <Transition Name="M2"/>
</System>""",
        encoding="utf-8",
    )
    findings = cvm.check_model(root)
    rules = sorted(f.split("VSM", 1)[1][:3] for f in findings)
    assert rules == ["001", "001", "001", "001", "002", "003", "003", "004"]
    assert cvm.main(["--root", str(root)]) == 1


def test_check_vs_model_without_model_passes(tmp_path: Path) -> None:
    root = _repo(tmp_path, model=False, gen=False)
    assert cvm.main(["--root", str(root)]) == 0


def test_check_vs_model_unreadable_xml(tmp_path: Path) -> None:
    root = _repo(tmp_path)
    (root / vc.MODEL_DIR / "WinCtrl.vsr").write_text("<System", encoding="utf-8")
    assert cvm.main(["--root", str(root)]) == 2


# --------------------------------------------------------------------------- constants


def test_parse_defines_and_macro_prefix() -> None:
    values = cvc.parse_defines(PARAMS + ENUMS)
    assert values["LS_T_WIN_BRAKE_MS"] == 100
    assert values["LS_TEMP_MIN_CDEG"] == -4000
    assert values["LS_WINDOW_STOP_REASON_DIR_MISMATCH"] == 16
    assert cvc.macro_prefix("WindowStopReason") == "LS_WINDOW_STOP_REASON_"


def test_check_vs_constants_fixture_is_consistent(tmp_path: Path) -> None:
    root = _repo(tmp_path)
    assert cvc.main(["--root", str(root)]) == 0


def test_check_vs_constants_findings(tmp_path: Path) -> None:
    root = _repo(tmp_path)
    project = root / vc.MODEL_DIR / "dcu_fsm.vsp"
    project.write_text(
        project.read_text()
        .replace('Name="kBrakeMs" Value="100"', 'Name="kBrakeMs" Value="90"')
        .replace('Name="DIR_MISMATCH" Value="16"', 'Name="DIR_MISMATCH" Value="15"')
        .replace(
            '<Literal Name="SAFE" Value="4"/>',
            '<Literal Name="SAFE" Value="4"/><Literal Name="BOGUS" Value="9"/>',
        ),
        encoding="utf-8",
    )
    params = cvc.parse_defines(PARAMS)
    enums = cvc.parse_defines(ENUMS)
    findings = cvc.check_model(vc.model_files(root), root, params, enums)
    assert len(findings) == 3
    assert any("kBrakeMs = 90, expected 100" in f for f in findings)
    assert any("WindowStopReason.DIR_MISMATCH = 15, expected 16" in f for f in findings)
    assert any("NodeMode.BOGUS has no LS_NODE_MODE_BOGUS" in f for f in findings)
    assert cvc.main(["--root", str(root)]) == 1


def test_check_vs_constants_missing_header_references(tmp_path: Path) -> None:
    root = _repo(tmp_path, model=False, gen=False)
    (root / vc.PARAMS_HEADER).write_text("", encoding="utf-8")
    assert cvc.main(["--root", str(root)]) == 1
    (root / vc.ENUMS_HEADER).unlink()
    assert cvc.main(["--root", str(root)]) == 2


def test_repository_headers_cover_the_checked_references() -> None:
    assert cvc.main(["--root", str(REPO_ROOT)]) == 0
