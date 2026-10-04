# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
"""Unit tests for tools/arch/check_layers.py."""

from __future__ import annotations

from pathlib import Path

import pytest

from tools.arch import check_layers as cl

REPO_ROOT = Path(__file__).resolve().parents[3]

CONFIG = """
version: 1
sources: [fw/src, fw/cfg]
include_roots: [fw/src, fw/cfg, libs/core/include, vendor/cmsis]
std_headers: [stdint.h]
groups:
  - {name: platform, glob: "fw/src/platform/*"}
  - {name: mcal, glob: "fw/src/mcal/{module}/*"}
  - {name: services, glob: "fw/src/services/{module}/*"}
  - {name: rte_view, glob: "fw/src/rte/rte_{module}.h"}
  - {name: rte, glob: "fw/src/rte/*"}
  - {name: swc, glob: "fw/src/app/{module}/*"}
  - {name: cfg, glob: "fw/cfg/{module}/*"}
  - {name: gen_vs, glob: "fw/gen_vs/{module}/*"}
  - {name: libs, glob: "libs/{module}/**"}
  - {name: cmsis, glob: "vendor/cmsis/**"}
allow:
  platform: [platform, cmsis]
  mcal: [platform, cmsis, "mcal:own", "cfg:mcal"]
  services: [platform, "mcal:wdg", services, libs]
  rte_view: [rte_view, platform]
  rte: [platform, rte, rte_view, services]
  swc: [platform, "rte_view:own", "swc:own", gen_vs, libs]
  cfg: [platform, mcal, services, cfg]
register_header_groups: [platform, mcal]
private_header_suffix: _priv.h
extern_object_groups: [rte]
extern_object_files: [fw/src/platform/ls_compiler.h]
vs_systems: {WinCtrl: win_ctrl}
"""

BASE_FILES = {
    "fw/src/platform/ls_std_types.h": "#include <stdint.h>\n",
    "fw/src/platform/ls_compiler.h": "extern uint32_t LsHost_Basepri;\n",
    "fw/src/mcal/gpio/gpio.h": '#include "platform/ls_std_types.h"\n',
    "fw/src/mcal/gpio/gpio.c": '#include "mcal/gpio/gpio.h"\n#include "stm32f1xx.h"\n',
    "fw/src/mcal/wdg/wdg.h": '#include "platform/ls_std_types.h"\n',
    "fw/src/services/wdgm/wdgm.c": '#include "mcal/wdg/wdg.h"\n#include "core/lib.h"\n',
    "fw/src/rte/rte_types.h": '#include "platform/ls_std_types.h"\n',
    "fw/src/rte/rte_win_ctrl.h": '#include "rte/rte_types.h"\n',
    "fw/src/rte/rte.c": '#include "rte/rte_win_ctrl.h"\nstatic int s_x;\n',
    "fw/src/app/win_ctrl/win_ctrl_priv.h": '#include "rte/rte_win_ctrl.h"\n',
    "fw/src/app/win_ctrl/win_ctrl.c": '#include "app/win_ctrl/win_ctrl_priv.h"\n',
    "fw/src/app/door_ctrl/door_ctrl.c": '#include "platform/ls_std_types.h"\n',
    "fw/cfg/mcal/gpio_cfg.c": '#include "mcal/gpio/gpio.h"\n',
    "fw/gen_vs/release/WinCtrl.h": "\n",
    "fw/gen_vs/release/DoorCtrl.h": "\n",
    "libs/core/include/core/lib.h": "\n",
    "vendor/cmsis/stm32f1xx.h": "\n",
}


def _tree(root: Path, extra: dict[str, str] | None = None) -> cl.Rules:
    files = dict(BASE_FILES)
    files.update(extra or {})
    for rel, text in files.items():
        path = root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
    config = root / "layers.yaml"
    config.write_text(CONFIG, encoding="utf-8")
    return cl.load_rules(config)


def _rules_of(violations: list[cl.Violation]) -> list[str]:
    return sorted(v.rule for v in violations)


def test_clean_tree_has_no_violation(tmp_path: Path) -> None:
    rules = _tree(tmp_path)
    assert cl.check_tree(tmp_path, rules) == []


def test_unclassified_file_is_reported(tmp_path: Path) -> None:
    rules = _tree(tmp_path, {"fw/src/stray.c": "\n"})
    assert _rules_of(cl.check_tree(tmp_path, rules)) == ["LAY001"]


def test_unresolved_include_is_reported(tmp_path: Path) -> None:
    rules = _tree(tmp_path, {"fw/src/mcal/gpio/gpio.c": '#include "missing.h"\n'})
    violations = cl.check_tree(tmp_path, rules)
    assert _rules_of(violations) == ["LAY002"]
    assert violations[0].line == 1


def test_upward_include_is_reported(tmp_path: Path) -> None:
    rules = _tree(tmp_path, {"fw/src/mcal/gpio/gpio.c": '#include "rte/rte_win_ctrl.h"\n'})
    violations = cl.check_tree(tmp_path, rules)
    assert _rules_of(violations) == ["LAY003"]
    assert "mcal:gpio may not include rte_view:win_ctrl" in violations[0].message


def test_mcal_module_outside_whitelist_is_reported(tmp_path: Path) -> None:
    rules = _tree(tmp_path, {"fw/src/services/wdgm/wdgm.c": '#include "mcal/gpio/gpio.h"\n'})
    violations = cl.check_tree(tmp_path, rules)
    assert _rules_of(violations) == ["LAY003"]
    assert "mcal:gpio" in violations[0].message


def test_other_swc_view_is_reported(tmp_path: Path) -> None:
    rules = _tree(tmp_path, {"fw/src/app/door_ctrl/door_ctrl.c": '#include "rte/rte_win_ctrl.h"\n'})
    assert _rules_of(cl.check_tree(tmp_path, rules)) == ["LAY003"]


def test_register_header_outside_mcal_is_reported(tmp_path: Path) -> None:
    rules = _tree(tmp_path, {"fw/src/app/door_ctrl/door_ctrl.c": '#include "stm32f1xx.h"\n'})
    assert _rules_of(cl.check_tree(tmp_path, rules)) == ["LAY004"]


def test_private_header_of_other_module_is_reported(tmp_path: Path) -> None:
    rules = _tree(
        tmp_path,
        {"fw/src/app/door_ctrl/door_ctrl.c": '#include "app/win_ctrl/win_ctrl_priv.h"\n'},
    )
    assert _rules_of(cl.check_tree(tmp_path, rules)) == ["LAY005"]


def test_extern_object_outside_rte_is_reported(tmp_path: Path) -> None:
    rules = _tree(
        tmp_path,
        {
            "fw/src/mcal/gpio/gpio.h": "extern int g_shared;\nextern const int k_table[2];\n"
            "extern void Gpio_Fn(void);\n"
        },
    )
    violations = cl.check_tree(tmp_path, rules)
    assert _rules_of(violations) == ["LAY006"]
    assert violations[0].line == 1


def test_vs_header_in_own_swc_is_allowed(tmp_path: Path) -> None:
    rules = _tree(
        tmp_path,
        {"fw/src/app/win_ctrl/win_ctrl.c": '#include "../../../gen_vs/release/WinCtrl.h"\n'},
    )
    assert cl.check_tree(tmp_path, rules) == []


def test_vs_header_in_other_swc_is_reported(tmp_path: Path) -> None:
    rules = _tree(
        tmp_path,
        {"fw/src/app/door_ctrl/door_ctrl.c": '#include "../../../gen_vs/release/WinCtrl.h"\n'},
    )
    assert _rules_of(cl.check_tree(tmp_path, rules)) == ["LAY007"]


def test_two_vs_headers_in_one_file_are_reported(tmp_path: Path) -> None:
    rules = _tree(
        tmp_path,
        {
            "fw/src/app/win_ctrl/win_ctrl.c": '#include "../../../gen_vs/release/WinCtrl.h"\n'
            '#include "../../../gen_vs/release/WinCtrl.h"\n'
        },
    )
    assert _rules_of(cl.check_tree(tmp_path, rules)) == ["LAY007"]


def test_glob_to_regex_module_and_wildcards() -> None:
    pattern = cl.glob_to_regex("a/{module}/*.c")
    match = pattern.match("a/gpio/gpio.c")
    assert match is not None
    assert match.group("module") == "gpio"
    assert pattern.match("a/gpio/sub/gpio.c") is None
    assert cl.glob_to_regex("libs/**").match("libs/x/y/z.h") is not None


@pytest.mark.parametrize(
    "text",
    [
        "version: 2\n",
        "version: 1\ngroups: [{name: a}]\n",
        "version: 1\ngroups: [{name: a, glob: x}]\nallow: {b: [a]}\n",
        "version: 1\ngroups: [{name: a, glob: x}]\nallow: {a: [c]}\n",
        "version: 1\nsources: x\n",
    ],
)
def test_invalid_configuration_is_rejected(tmp_path: Path, text: str) -> None:
    config = tmp_path / "layers.yaml"
    config.write_text(text, encoding="utf-8")
    with pytest.raises(cl.ConfigError):
        cl.load_rules(config)


def test_main_exit_codes(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    _tree(tmp_path)
    assert cl.main(["--config", str(tmp_path / "layers.yaml"), "--root", str(tmp_path)]) == 0
    _tree(tmp_path, {"fw/src/stray.c": "\n"})
    assert cl.main(["--config", str(tmp_path / "layers.yaml"), "--root", str(tmp_path)]) == 1
    assert "LAY001" in capsys.readouterr().out
    assert cl.main(["--config", str(tmp_path / "missing.yaml")]) == 2


def test_repository_dcu_tree_follows_the_layer_rules() -> None:
    rules = cl.load_rules(cl.DEFAULT_CONFIG)
    assert cl.check_tree(REPO_ROOT, rules) == []
