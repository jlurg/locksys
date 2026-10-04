# IAR Visual State Modelling Guide (DCU)

| Field | Value |
|---|---|
| Document ID | LS-DCU-GDE-001 |
| Version | 0.1 |
| Status | Draft |
| Owner | jlurg |
| Applies to | WinCtrl, DoorCtrl and ModeMgr of the DCU; IAR Visual State 11.2.1 or later on the Windows Lab Host |

## 1. Purpose and scope

This guide gives the procedures and rules for the Visual State model of the DCU: creating the project, modelling conventions, the coder option files, code generation, model verification and validation, animation on the target, what is committed, and the checks that run in CI.

- The behaviour to be modelled (states, events, guards, actions and transition identifiers) is specified in [LS-DCU-SAD-001](architecture.md) §8. This guide does not repeat it.
- Decision record: [ADR 0013](../../adr/0013-iar-visual-state-for-dcu-state-machines.md). Lab Host set-up and day-1 checks: [Lab Host](../../08_process/lab_host.md). MISRA treatment of generated code: [Guideline Enforcement Plan](../../08_process/misra/gep.md).
- Visual State runs only on the Lab Host; model editing, the Validator, the Documenter and C-SPYLink animation are used over remote desktop.

## 2. Files

| Path | Content | Committed |
|---|---|---|
| `firmware/dcu/model/visualstate/dcu_fsm.vnw` | Workspace; stores the coder selection | Yes |
| `firmware/dcu/model/visualstate/dcu_fsm.vsp` | Project with three systems and the project-level enumerations | Yes |
| `firmware/dcu/model/visualstate/WinCtrl.vsr`, `DoorCtrl.vsr`, `ModeMgr.vsr` | One system file per state machine | Yes |
| `firmware/dcu/model/visualstate/options/coder_release.opt`, `coder_debug.opt` | Coder options of the two generated variants | Yes |
| `firmware/dcu/model/visualstate/options/verificator.opt` | Verificator options | Yes |
| `firmware/dcu/model/visualstate/validator/dcu_fsm.vws`, `validator/sequences/*.vxlg` | Validator workspace and recorded sequences | Yes |
| `firmware/dcu/model/visualstate/reports/` | Verificator and Validator reports of local runs | No (ignored) |
| `firmware/dcu/gen_vs/release/`, `firmware/dcu/gen_vs/debug/` | Generated engines | Yes, never edited |
| `firmware/dcu/gen_vs/VS_MANIFEST.json` | Hashes of model and generated code, Visual State version, coder options, Verificator summary | Yes |
| `firmware/dcu/scripts/vs/Invoke-VsGenerate.ps1`, `Invoke-VsVerify.ps1` | Generation and verification scripts | Yes |
| `tools/vs/vs_manifest.py`, `check_vs_gen.py`, `check_vs_model.py`, `check_vs_constants.py` | CI checks | Yes |

Model files are XML with identifiers and layout data. `.gitattributes` marks them `-text merge=binary`, so they are never merged textually: one person edits a `.vsr` file at a time, and changes are reviewed with the IAR Compare tool.

## 3. Prerequisites

1. Lab Host checks D7 (Visual State version, licence, headless use), D8 (C-SPYLink plugin) and D9 (first generation) are recorded in the [Lab Host](../../08_process/lab_host.md) table. Before D9 is GO, generated code is not used in the firmware build.
2. Visual State 11.2.1 or later is installed; the installed version is recorded in `tools/versions.env` with the pin-change procedure of [Toolchains](../../08_process/toolchains.md).
3. The developer clone `C:\dev\locksys` is on a work branch named according to the [branching model](../../08_process/branching.md), and the branch is edited only on the Lab Host for the duration of the model change.
4. `C:\labhost\bench.lock` exists for the duration of the remote desktop session.
5. If EWARM was installed after Visual State, the C-SPYLink plugin is copied into the EWARM installation as described in §10.1.

## 4. Procedure A: create the project

This procedure runs once (Lab Host step U5). Menu names are those of Visual State 11.x.

1. Start the Visual State Navigator and create a workspace `dcu_fsm.vnw` with a project `dcu_fsm.vsp` in `firmware/dcu/model/visualstate/`.
2. Select the Classic Coder: **Project > Options > Code Generation > Switch Coder**, then **Classic Coder**. New projects default to the Hierarchical Coder, which dispatches through function-pointer tables and is not permitted. Save the workspace; the selection is stored in `dcu_fsm.vnw`.
3. Add three systems named exactly `WinCtrl`, `DoorCtrl` and `ModeMgr`, each in its own `.vsr` file in the same directory and each with one top-level state machine. The system names become the API prefixes (`WinCtrlVSDeduct`).
4. Declare the project-level enumerations with the contract names, literal names and values of LS-SAIC-001 §6.4: `NodeMode`, `WindowState`, `WindowStopReason`, `DoorLockState` and `CommandResult`. Value 0 is the safe or unknown value. `tools/vs/check_vs_constants.py` compares them with `libs/ls_common/gen/ls_enums_gen.h`.
5. Leave the code-generation fields of the GUI empty. Code is generated only with the committed option files (§7); the GUI generation command is never used.
6. Model the three systems as specified in LS-DCU-SAD-001 §8.9 (WinCtrl), §8.10 (DoorCtrl) and §8.11 (ModeMgr), following the conventions of §5.
7. Generate (§7), verify (§8) and validate (§9), then commit (§11).

## 5. Modelling conventions

### 5.1 Names

| Element | Form | Scope | Example |
|---|---|---|---|
| Event | `ev<Name>` | System | `evReqUp`, `evTick`, `evSafe` |
| Timer event | `evTm<Name>` | System | `evTmBrake`, `evTmSettle` |
| Action function | `a<Verb><Object>` | System | `aBridgeBrake`, `aStopPress` |
| Timer action function | `aTmStart(event, ticks)`; the generated `_stop` variant stops a timer | System | `aTmStart(evTmBrake, kBrakeMs)` |
| Trace action | `aTr(kTr<ID>)` | System | `aTr(kTrW6)` |
| External variable (input) | `x<Name>` | System | `xStartOk`, `xRunOk` |
| Internal variable | `v<Name>` | System | `vAttempts`, `vDir` |
| Constant | `k<Name>` | System or project | `kBrakeMs`, `kTrW6` |
| Enumeration | Contract name, verbatim | Project | `WindowStopReason` |
| Transition | Name or comment equal to its identifier | — | `W6`, `D10`, `M4` |
| Signal | `sg<Name>` | System | Not used in stage A |

### 5.2 Rules

1. Guards contain only tests of external or internal variables against constants or enumeration literals: no function calls, no arithmetic. Complex conditions are computed in C (`<swc>_guards.c`) and passed as external variables.
2. Outputs are produced only by actions. Actions write the output buffer of the SWC; they never call the RTE, an ECUAL module or a deduct function.
3. The first action of every transition is `aTr(kTr<ID>)` with the numeric identifier of LS-DCU-SAD-001 §8.8 (Wk → k, Dk → 20 + k, Mk → 40 + k).
4. Every non-idle state has a bounded exit: a timer or an event that is guaranteed to occur.
5. No cycles of trigger-less transitions.
6. Events have no parameters; data reaches the model through external variables.
7. `VSForceState` and other state-forcing API calls are not used in product code.
8. Model constants that mirror parameters (LS-DCU-SAD-001 §8.5) carry the same value as the parameter; `tools/vs/check_vs_constants.py` checks them.
9. Each machine has at most 12 states and 30 transitions.
10. A transition drawn on a composite state is used for "from any substate" behaviour (for example W12 and W13 on `Operational`), instead of repeating the transition per substate.

### 5.3 Element checklist per system

| System | Events in priority order | External variables | Internal variables | Timers |
|---|---|---|---|---|
| WinCtrl | `evSafe`, `evDriverFault`, `evLimitUp` (B), `evLimitDn` (B), `evMotionFault`, `evDriveRefused`, `evTmBrake`, `evTmDead`, `evTick`, `evReqUp`, `evReqDown` | `xModeReady`, `xStartOk`, `xRunOk`, `xWinFaultLatched`, `xFaultCleared` | — | `evTmBrake` (`kBrakeMs`), `evTmDead` (`kRevDeadMs`) |
| DoorCtrl | `evSafe`, `evPulseEnd`, `evTmPulseGuard`, `evTmSettle`, `evTmPause`, `evTick`, `evDoorReq` | `xFbDebounced`, `xDoorStartOk`, `xAlreadyInTarget`, `xPulseFault`, `xFbAtTarget`, `xRetryOk` | `vAttempts`, `vDir` | `evTmPulseGuard` (`kPulseGuardMs`), `evTmSettle` (`kSettleMs`), `evTmPause` (`kRetryPauseMs`) |
| ModeMgr | `evSafeReq`, `evTmInitMax`, `evNonCritFault`, `evHealedNow`, `evTmHeal`, `evSelfTestOk` | `xDegradedActive` | — | `evTmInitMax` (`kInitMaxMs`), `evTmHeal` (`kModeHealMs`) |

The event order of the table is the drain order of the adapter; declaring the events in this order keeps the generated event numbers aligned with the adapter bit numbers.

## 6. Option files

The options of both generated variants live in `firmware/dcu/model/visualstate/options/`. Rules of the option file format:

- one option per line; `//` starts a comment;
- project options may appear anywhere; system options placed before any `-V<System>` line apply to all systems;
- `-D` exists only in option files and on the command line, not in the GUI.

| Option | Release (`coder_release.opt`) | Debug (`coder_debug.opt`) | Purpose |
|---|---|---|---|
| `-api_type0 -readable1` | Yes | Yes | Adaptive API, readable code with direct calls |
| `-useheap0` | Yes | Yes | No `malloc`; static data |
| `-maximummisra1` | Yes | Yes | `VSResult` enumeration, no `VS_TRUE`/`VS_FALSE` |
| `-useapiprefix1 -apiprefix$(SYSNAME)` | Yes | Yes | API names `<System>VS…` |
| `-typestyle1` | Yes | Yes | `<stdint.h>` types where possible |
| `-D2` | Yes | Yes | 32-bit SEM types |
| `-semnextstatechg0` | Yes | Yes | `SES_FOUND` not used as a state-change report |
| `-generatetimeandversion0` | Yes | Yes | Byte-identical regeneration |
| `-warnings_are_errors1 -warnings_affect_exit_code1` | Yes | Yes | A coder warning stops generation |
| `-omitcontradictiontests0` | Yes | Yes | Contradiction tests stay in the code (confirmed by inspection at check D9) |
| `-path<dir>` | `..\..\gen_vs\release\` | `..\..\gen_vs\debug\` | Output directory |
| `-cspylink` | `0` | `1` | C-SPYLink instrumentation |
| C-SPYLink buffers | — | `-fullinstrumentation0 -usesamplingbuffer1 -samplingbuffersize32 -userecordingbuffer1 -recordingbuffersize128` | Animation with bounded RAM and run-time cost |

The exact option spellings are confirmed against the option list printed by `Coder.exe` without arguments; a difference is corrected in the option file, never in the GUI.

## 7. Procedure B: generate code

1. Save all model files in the Navigator.
2. In PowerShell 7, in `C:\dev\locksys`:

   ```powershell
   ./firmware/dcu/scripts/vs/Invoke-VsGenerate.ps1
   ```

   The script runs `Coder.exe dcu_fsm.vsp --@<option file>` for the release and debug variants, writes `firmware/dcu/gen_vs/release/` and `firmware/dcu/gen_vs/debug/`, and updates `firmware/dcu/gen_vs/VS_MANIFEST.json`. Its parameters are documented in its comment-based help (`Get-Help ./firmware/dcu/scripts/vs/Invoke-VsGenerate.ps1 -Full`).
3. Run the repository checks:

   ```powershell
   uv run tools/vs/vs_manifest.py --check
   uv run tools/vs/check_vs_model.py
   uv run tools/vs/check_vs_gen.py
   uv run tools/vs/check_vs_constants.py
   ```

4. Build the Debug, Hil and Release configurations ([IAR project setup guide](iar_project_setup.md)) and run the unit tests (`tools/docker/ceedling/run.sh firmware/dcu test:all` on the workstation).
5. `./firmware/dcu/scripts/vs/Invoke-VsGenerate.ps1 -Check` regenerates into a temporary location and compares with the committed code; CI uses this form (§12).

## 8. Procedure C: verify the model

1. Run the Verificator in full mode for each system:

   ```powershell
   ./firmware/dcu/scripts/vs/Invoke-VsVerify.ps1
   ```

   The script calls `Verificator.exe dcu_fsm.vsp <System>` with `options/verificator.opt` for WinCtrl, DoorCtrl and ModeMgr and writes the reports to `firmware/dcu/model/visualstate/reports/`.
2. Pass criteria:
   - zero critical findings (conflicting transitions, ambiguous assignments, signal queue size);
   - project policy: no dead ends except the terminal state `Safe` of ModeMgr; no unused events, variables, constants or actions.
3. The script records the result per system in `VS_MANIFEST.json`. A non-critical finding that is accepted is listed with its justification in the pull request.

## 9. Procedure D: validate the model

1. Open `firmware/dcu/model/visualstate/validator/dcu_fsm.vws` in the Validator.
2. Maintain one recorded sequence per transition identifier, named `<ID>.vxlg` (for example `W6.vxlg`), plus the scenario sequences below, named `scn_<name>.vxlg`.

   | Scenario | Transitions covered |
   |---|---|
   | `scn_press_release` | W1, W2, W6, W9, W11 |
   | `scn_reversal_newer_press` | W2, W6, W9, W11, W3 |
   | `scn_start_refused` | W4, W5 |
   | `scn_motion_fault` | W7, W9 |
   | `scn_dir_mismatch_clear` | W7, W10, W15, W11 |
   | `scn_driver_fault` | W12, W15 |
   | `scn_refused_drive` | W2, W8 |
   | `scn_safe_while_moving` | W13 |
   | `scn_safe_at_init` | W14 |
   | `scn_door_ok_first_pulse` | D1, D2, D5, D8 |
   | `scn_door_retry_ok` | D2, D5, D9, D11, D5, D8 |
   | `scn_door_retry_fail` | D9, D11, D10 |
   | `scn_door_in_target_and_rejects` | D3, D4, D14 |
   | `scn_door_busy_drop` | D15 |
   | `scn_door_pulse_fault_guard_safe` | D6, D7, D12, D13 |
   | `scn_mode_init_normal_degraded_heal` | M2, M5, M6, M7, M8 |
   | `scn_mode_degraded_and_safe` | M3, M4, M1 |

3. Pass criterion: the coverage report shows 100 % of states, transitions, events and actions for each system.
4. Store the coverage report of the release candidate with the release evidence ([release process](../../08_process/release_process.md)). Every engine unit test mirrors one sequence and carries the transition identifier in its name.

## 10. Procedure E: animate on the target (Debug configuration)

### 10.1 One-time set-up

1. Check that `vs.ewplugin` is present in `$EW_DIR$\common\plugins` of the EWARM installation. If EWARM was installed after Visual State, copy the plugin from the Visual State installation and check that its `<dllFile>` entry and the Validator C-SPY library name match the EWARM version (check D8).
2. In the IAR project, Debug configuration only: **Project > Options > Debugger > Plugins**, enable the Visual State plugin. The Hil and Release configurations never load it.

### 10.2 Session

1. Bench safety: the 12 V supply output is off, or its current limit is at most 0.5 A. A halted CPU also halts the software protections, and INA/INB keep their state.
2. Build the Debug configuration (it links `gen_vs/debug/` with C-SPYLink instrumentation) and start a C-SPY debug session from the IAR IDE over remote desktop.
3. Open the C-SPYLink window, connect it to `dcu_fsm.vsp` and start execution. One hardware breakpoint is reserved for C-SPYLink.
4. Expect the CGW_WinCmd RX timeout and the alive supervision to react after every halt; that is the specified behaviour, not a defect.
5. Animation inside the VS Code C-SPY extension is not assumed; check D8 records whether it works.

## 11. What to commit

| Commit | Do not commit |
|---|---|
| `.vnw`, `.vsp`, `.vsr` | `*.vsviewinfo`, `*.vsgaviewinfo` |
| `options/*.opt` | `reports/` |
| `validator/dcu_fsm.vws`, `validator/sequences/*.vxlg` | GUI-generated `.vtg` files |
| `gen_vs/release/**`, `gen_vs/debug/**`, `gen_vs/VS_MANIFEST.json` | Validator temporary logs |
| Documenter output for tagged releases (converted diagrams under `docs/04_software/dcu/swdd/img/`) | — |

Rules:

- A model change, its regenerated code and the updated manifest are committed together, in one commit of the same pull request.
- Generated files are never edited. A required change is made in the model or in an option file, followed by regeneration.
- The pull request lists the transitions added, removed or changed by identifier, and the Validator sequences updated for them.

## 12. Checks in CI

| Check | Job (required check) | Runner | Command | Fails when |
|---|---|---|---|---|
| Manifest | `vs-manifest` (`ci-gate`) | GitHub-hosted | `uv run tools/vs/vs_manifest.py --check` | The model changed without regeneration, or generated code was edited |
| Model rules | `vs-manifest` (`ci-gate`) | GitHub-hosted | `uv run tools/vs/check_vs_model.py` | An event has parameters; a transition lacks `aTr`; naming rules are violated |
| Generated code rules | `vs-manifest` (`ci-gate`) | GitHub-hosted | `uv run tools/vs/check_vs_gen.py` | `va_arg`, `malloc` or `free` appear in `gen_vs/release/` |
| Constants | `vs-manifest` (`ci-gate`) | GitHub-hosted | `uv run tools/vs/check_vs_constants.py` | A model constant or enumeration differs from the generated parameter or enumeration header |
| Regeneration | `iar-gate` | Lab Host `iar` | `./firmware/dcu/scripts/vs/Invoke-VsGenerate.ps1 -Check` | Regenerated code differs from the committed code |
| Verificator | `iar-gate` | Lab Host `iar` | `./firmware/dcu/scripts/vs/Invoke-VsVerify.ps1` | A critical finding, or a dead end other than `Safe` (from M2) |
| C-STAT on `gen_vs/release/` | `iar-gate` | Lab Host `iar` | Part of `Invoke-DcuBuild.ps1 -CStat` | An unsuppressed Mandatory or Required finding outside permit DP-05 |
| Engine unit tests | `dcu-unit` (`ci-gate`) | GitHub-hosted | `tools/docker/ceedling/run.sh firmware/dcu test:all` | A test fails or transition coverage is below 100 % |
| Layer rules | `dcu-static` (`ci-gate`) | GitHub-hosted | `uv run tools/arch/check_layers.py` | A generated system header is included outside its SWC, or two in one translation unit |

The Lab Host jobs run only for `push` events; see [CI/CD](../../08_process/ci_cd.md).

## 13. First-generation checklist (check D9)

Record each result in the D9 row of the [Lab Host](../../08_process/lab_host.md) day-1 table.

| Item | Expected | If not met |
|---|---|---|
| Coder in use | Classic Coder, readable code | Fix the workspace selection (§4 step 2) |
| Generated API names | `WinCtrlVSInitAll`, `WinCtrlVSDeduct`, and the same for DoorCtrl and ModeMgr | Fix the API prefix options |
| Function pointers | None in `gen_vs/release/` | Contingency of the [deviation process](../../08_process/misra/deviation_process.md); ADR required |
| Heap | No `malloc` or `free` | Fix `-useheap0` |
| `<System>VSDeduct` | Declared variadic; `va_arg` never used | Record a Rule 17.1 entry for permit DP-05 if `va_list` or `va_start` appear |
| Contradiction tests | Present in the generated deduct function | Review the `-omitcontradictiontests` setting |
| External variables | Defined by the generated code | Add `src/app/<swc>/<swc>_vsext.c` with the definitions |
| Enumerations | Contract names and values; file placement recorded | Adjust the model enumerations |
| Types | `<stdint.h>` types | Use `-typestyle2` with explicit type mapping |
| C-STAT baseline of `gen_vs/release/` | Findings listed per guideline | The list defines permit DP-05; a Mandatory finding triggers §15 |

## 14. Model change procedure

1. Create a work branch on the Lab Host; announce the `.vsr` files being edited in the issue (one editor per file).
2. Edit the model; keep transition identifiers stable. A removed transition identifier is not reused; a new transition takes the next free identifier of its machine and is added to LS-DCU-SAD-001 §8 in the same pull request.
3. Run procedures B, C and D; update the affected Validator sequences and engine unit tests.
4. Commit the model, the regenerated code and the manifest together; push; open the pull request.
5. Review the model change with the IAR Compare tool and the generated C diff.

## 15. Fallback: model as specification

If a Mandatory MISRA guideline, or a Required guideline with real risk (for example Rules 13.2 or 17.2), is violated by generated code and no coder option or model change removes the finding, the affected SWC changes to "model as specification": the model remains the design and validation artefact, the C code of that SWC is written by hand from it as an enumeration-and-`switch` state machine, the Validator sequences are ported to unit tests, and `gen_vs/` no longer contains that system. The decision is recorded in an ADR.

## 16. References

- [LS-DCU-SAD-001 DCU software architecture](architecture.md) §8
- [ADR 0013: IAR Visual State for DCU state machines](../../adr/0013-iar-visual-state-for-dcu-state-machines.md)
- [Guideline Enforcement Plan](../../08_process/misra/gep.md), [deviation process](../../08_process/misra/deviation_process.md)
- [Lab Host](../../08_process/lab_host.md), [CI/CD](../../08_process/ci_cd.md), [Toolchains](../../08_process/toolchains.md)
- [IAR project setup guide](iar_project_setup.md)
- IAR Visual State User Guide (Classic Coder, Adaptive API, Coder and Verificator command lines, C-SPYLink)
