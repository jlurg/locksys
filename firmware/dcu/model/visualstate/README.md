# Visual State model of the DCU

| Field | Value |
|---|---|
| Systems | WinCtrl, DoorCtrl, ModeMgr (one `.vsr` file each) in project `dcu_fsm.vsp` |
| Tool | IAR Visual State 11.2.1 or later (`IAR_VISUAL_STATE_MIN`), Classic Coder, Adaptive API, readable code |
| Behaviour | [LS-DCU-SAD-001](../../../../docs/04_software/dcu/architecture.md) section 8 |
| Procedures | [Visual State modelling guide](../../../../docs/04_software/dcu/visual_state_guide.md) (LS-DCU-GDE-001) |
| Status | No model yet: the workspace and project are created on the Lab Host (procedure A) |

| Path | Content | Committed |
|---|---|---|
| `dcu_fsm.vnw`, `dcu_fsm.vsp`, `WinCtrl.vsr`, `DoorCtrl.vsr`, `ModeMgr.vsr` | Workspace, project and systems | Yes |
| `options/coder_release.opt` | Coder options of `gen_vs/release/` (Hil, Release, unit tests) | Yes |
| `options/coder_debug.opt` | Coder options of `gen_vs/debug/` (Debug, C-SPYLink) | Yes |
| `options/verificator.opt` | Verificator options | Yes |
| `validator/` | Validator workspace and recorded sequences | Yes |
| `reports/` | Verificator and Validator reports of local runs | No |

Code is generated only through `firmware/dcu/scripts/vs/Invoke-VsGenerate.ps1` with the
committed option files, never with the generation command of the GUI. A model change, the
regenerated code and `firmware/dcu/gen_vs/VS_MANIFEST.json` are committed together. CI runs
`uv run tools/vs/vs_manifest.py --check`, `check_vs_model.py`, `check_vs_gen.py` and
`check_vs_constants.py`.
