# IAR Project Setup Guide (DCU)

| Field | Value |
|---|---|
| Document ID | LS-DCU-GDE-002 |
| Version | 0.1 |
| Status | Draft |
| Owner | jlurg |
| Applies to | IAR Embedded Workbench for Arm 9.70 baseline (10.10 only if the licence allows) on the Windows Lab Host |

## 1. Purpose and scope

This guide gives the procedure for creating the IAR workspace and project of the DCU firmware in the EWARM IDE on the Lab Host, and the settings the project must have. IAR project files are written by the IDE only; they are never written or edited by hand ([Lab Host](../../08_process/lab_host.md) working rule 4).

- Architecture, layers and build configurations: [LS-DCU-SAD-001](architecture.md) §3, §12 and §14.
- MISRA analysis: [Guideline Enforcement Plan](../../08_process/misra/gep.md) and the [DCU MISRA compliance record](misra_compliance.md).
- Visual State engines: [Visual State modelling guide](visual_state_guide.md).

## 2. Prerequisites

1. Lab Host checks D1 to D4 (EWARM version, C-STAT, licence under `labrunner`, licence terms) are recorded in the [Lab Host](../../08_process/lab_host.md) table, and the exact EWARM build is recorded in `tools/versions.env` (`IAR_EWARM_BASELINE`). The project is created with that EWARM line; opening it with another line can convert the files, and converted files are never committed.
2. The developer clone `C:\dev\locksys` is on a work branch, with the DCU source tree, `third_party/` (CMSIS-Core 5.9.0 and `cmsis_device_f1` v4.3.5), `libs/` and the generated code present.
3. `C:\labhost\bench.lock` exists while the IDE is used over remote desktop.

## 3. Files and locations

| File | Location | Committed |
|---|---|---|
| Workspace `dcu.eww` | `firmware/dcu/iar/` | Yes |
| Project `dcu.ewp` | `firmware/dcu/iar/` | Yes |
| Debugger settings `dcu.ewd` | `firmware/dcu/iar/` (created by the IDE) | Yes |
| C-STAT and C-RUN settings `dcu.ewt` | `firmware/dcu/iar/` (created by the IDE) | Yes |
| C-SPY macro `dcu_debug.mac` | `firmware/dcu/iar/cspy/` | Yes |
| Linker configuration and stack-usage control file | `firmware/dcu/linker/` | Yes |
| IDE state | `firmware/dcu/iar/settings/` | No (ignored) |
| Build outputs | `firmware/dcu/iar/Debug/`, `Hil/`, `Release/` | No (ignored) |
| Dependency files | `*.dep` | No (ignored) |

`$PROJ_DIR$` is `firmware/dcu/iar`. Every path in the project is relative to `$PROJ_DIR$`; absolute paths are not allowed. `.gitattributes` stores the IAR files with CRLF line endings.

## 4. Procedure

### 4.1 Create the workspace and the project

1. Start EWARM. **File > New Workspace**.
2. **Project > Create New Project**, toolchain Arm, template "Empty project"; save it as `firmware\dcu\iar\dcu.ewp`.
3. **File > Save Workspace As** `firmware\dcu\iar\dcu.eww`.
4. **Project > Edit Configurations**: keep `Debug` and `Release`; add `Hil` based on `Release`.

### 4.2 Create the groups

Groups mirror the source folders ([LS-DCU-SAD-001](architecture.md) §3.1). Add the `.c` files of each folder to its group; headers need not be added.

| Group | Sub-groups | Files from |
|---|---|---|
| `startup` | — | `firmware/dcu/startup/` |
| `platform` | — | `firmware/dcu/src/platform/` |
| `mcal` | One per module (`clk`, `gpio`, `nvic`, `exti`, `stk`, `can`, `i2c`, `uart`, `dma`, `pwm`, `adc`, `enc`, `wdg`, `crc`, `pwr`) | `firmware/dcu/src/mcal/<module>/` |
| `ecual` | One per module | `firmware/dcu/src/ecual/<module>/` |
| `services` | One per module | `firmware/dcu/src/services/<module>/` |
| `rte` | — | `firmware/dcu/src/rte/` |
| `app` | `win_ctrl`, `door_ctrl`, `mode_mgr`, `temp_mon`, `cmd_arb`, `diag_hdl` | `firmware/dcu/src/app/<swc>/` |
| `cfg` | `mcal`, `ecual`, `services` | `firmware/dcu/cfg/<layer>/` |
| `gen` | — | `firmware/dcu/gen/` (CAN pack and unpack, DTC table) |
| `libs` | `ls_e2e`, `ls_common` | `libs/ls_e2e/src/`, `libs/ls_common/src/` |
| `vs_engine` | `release`, `debug` | `firmware/dcu/gen_vs/release/`, `firmware/dcu/gen_vs/debug/` |
| (project root) | — | `firmware/dcu/src/main.c` |

`vs_engine` per configuration: right-click the `debug` sub-group, **Options**, check **Exclude from build** for `Hil` and `Release`; right-click the `release` sub-group and exclude it from `Debug`. A new source file is added to its group in the same pull request that adds the file.

### 4.3 Project options (all configurations)

Select the project node, **Project > Options**, with **All configurations** selected unless a value differs per configuration.

| Category | Page | Setting |
|---|---|---|
| General Options | Target | Device: ST STM32F103RB; little-endian; no FPU |
| General Options | Output | Executable; output directories `$CONFIG_NAME$\Exe`, `$CONFIG_NAME$\Obj`, `$CONFIG_NAME$\List` |
| General Options | Library Configuration | DLIB Normal; low-level interface implementation None (no semihosting); "Use CMSIS" off (CMSIS is vendored) |
| C/C++ Compiler | Language 1 | Language C; C dialect Standard C; language conformance "Standard with IAR extensions"; variable-length arrays off; "Require prototypes" on (`--require_prototypes`) |
| C/C++ Compiler | Language 2 | Plain `char` unsigned |
| C/C++ Compiler | Optimizations | Debug: Low. Hil and Release: High, Balanced (identical, so that HIL timing evidence is representative) |
| C/C++ Compiler | Output | Generate debug information on, in every configuration |
| C/C++ Compiler | List | Output list file on (Debug) |
| C/C++ Compiler | Preprocessor | Include directories and defined symbols of §4.4 |
| C/C++ Compiler | Diagnostics | Enable remarks on; "Treat all warnings as errors" on; no suppressed diagnostics |
| Assembler | Diagnostics | "Treat all warnings as errors" on |
| Output Converter | Output | "Generate additional output" off; the `.hex` is produced after the checksum by the post-build step (§4.6) |
| Build Actions | Pre-build and post-build command lines | §4.6 |
| Linker | Config | Override default: `$PROJ_DIR$\..\linker\dcu_stm32f103rb.icf` |
| Linker | Library | Automatic runtime library selection on; entry `__iar_program_start` |
| Linker | Input | Keep symbol `ls_rom_crc` |
| Linker | Advanced | Stack usage analysis on, control file `$PROJ_DIR$\..\linker\dcu.suc` |
| Linker | Output | Output file `dcu.out`; include debug information |
| Linker | List | Generate linker map file on (module summary and symbol listing) |
| Linker | Diagnostics | "Treat all warnings as errors" on |
| Linker | Checksum | Fill and checksum off; both are done by the post-build step |
| Linker | Extra Options | `--place_holder ls_rom_crc,4,.checksum,4` |

Language extensions are needed by `ls_compiler.h` (`__no_init`, section placement, call-graph roots) and by the CMSIS headers. Their use in project code outside `ls_compiler.h` is detected by C-STAT through MISRA Rule 1.2 (adopted as Required) with permit DP-02.

Stack and heap: the linker configuration defines CSTACK (2 KB at 0x20000000), the `noinit` area, FAULT_STACK and no heap block; a `malloc` reference therefore fails to link. Stack and heap sizes are not set in the IDE.

### 4.4 Include paths and defined symbols

Project-level include directories (all configurations):

```text
$PROJ_DIR$\..\src
$PROJ_DIR$\..\cfg
$PROJ_DIR$\..\gen
$PROJ_DIR$\..\..\..\libs\ls_e2e\include
$PROJ_DIR$\..\..\..\libs\ls_common\include
$PROJ_DIR$\..\..\..\libs\ls_common\gen
$PROJ_DIR$\$CONFIG_NAME$\gen
```

Per configuration, add the Visual State variant:

| Configuration | Additional include directory |
|---|---|
| Debug | `$PROJ_DIR$\..\gen_vs\debug` |
| Hil, Release | `$PROJ_DIR$\..\gen_vs\release` |

Register headers are visible only to the groups `startup`, `platform` and `mcal`. For each of these groups: right-click the group, **Options**, **C/C++ Compiler**, check **Override inherited settings** on the Preprocessor page and enter the complete project-level list plus:

```text
$PROJ_DIR$\..\..\..\third_party\cmsis_core-5.9.0\CMSIS\Core\Include
$PROJ_DIR$\..\..\..\third_party\cmsis_device_f1-4.3.5\Include
```

The paths follow the layout recorded in `third_party/README.md`. A group override replaces the inherited list, so the group lists repeat the project-level entries. MCAL public headers do not include the device header, so the upper layers compile without the CMSIS paths; an include of `stm32f1xx.h` outside these groups fails to compile.

Defined symbols:

| Symbol | Debug | Hil | Release |
|---|---|---|---|
| `STM32F103xB` | defined | defined | defined |
| `LS_CFG_DET` | 1 | 1 | 0 |
| `LS_CFG_FI` | 1 | 1 | 0 |
| `LS_CFG_TRACE` | 1 | 1 | 0 |
| `LS_CFG_TRCOV` | 1 | 1 | 0 |
| `LS_CFG_ROMCRC_ENFORCE` | 0 | 1 | 1 |

### 4.5 Linker configuration file

The project uses the committed `.icf` file only. Its content is defined by [LS-DCU-SAD-001](architecture.md) §11.1:

- application region 0x08000000–0x0801EFFB; `.checksum` section with `ls_rom_crc` at 0x0801EFFC; 0x0801F000–0x0801FFFF never used;
- CSTACK block of 2 KB at 0x20000000; `noinit` region of 256 B above it (`do not initialize`); data and FAULT_STACK above;
- a `check that` rule requiring CSTACK to cover the maximum stack of `Program entry` plus all `interrupt` call-graph roots plus a margin.

### 4.6 Build actions

| Action | Command line | Effect |
|---|---|---|
| Pre-build | `pwsh -NoProfile -ExecutionPolicy Bypass -File "$PROJ_DIR$\..\scripts\iar\New-BuildInfo.ps1" -OutDir "$PROJ_DIR$\$CONFIG_NAME$\gen" -Config "$CONFIG_NAME$"` | Writes the version header (version, git hash, dirty flag, BuildType) through the repository version generator; Debug and Hil always report DEV |
| Post-build | `pwsh -NoProfile -ExecutionPolicy Bypass -File "$PROJ_DIR$\..\scripts\iar\Invoke-PostLink.ps1" -Elf "$TARGET_PATH$" -Config "$CONFIG_NAME$"` | Runs `ielftool --fill "0xFF;0x08000000-0x0801EFFB" --checksum "ls_rom_crc:4,crc32:Li,0xFFFFFFFF;0x08000000-0x0801EFFB"` on `dcu.out`, then produces `dcu.hex` from the checksummed image |

- The script quotes the `ielftool` arguments: `;` separates commands in PowerShell.
- The released `.hex` and `.out` are always the post-`ielftool` files; CI recomputes the CRC.
- The IDE may skip the pre-build step of an up-to-date make; CI and release builds always run a full `-build`.

### 4.7 C-STAT

1. **Project > Options > Static Analysis > C-STAT**: select the checks of the gating rule set of the [Guideline Enforcement Plan](../../08_process/misra/gep.md): the MISRA C:2023 package for MISRA C:2012 Amendments 1–4 (Mandatory, Required and the adopted Advisory guidelines), the CERT C subset and the standard checks. The selection is stored in `dcu.ewt`.
2. Apply the selection to the Hil and Release configurations. The Debug configuration may run C-STAT but is not gated.
3. Path-scoped permits (DP-01 to DP-05) are applied in the C-STAT configuration of `firmware/dcu/cstat/`, never with comments in generated code. On EWARM 10.10.x the YAML configuration file of that directory is passed to the command-line analysis; on 9.70.x the `.ewt` selection is the analysis configuration and the enabled-check list is exported to `firmware/dcu/cstat/` for review.
4. Run **Project > C-STAT Static Analysis > Analyze Project** for Release and Hil and check that the report lists zero unsuppressed findings, or that every finding has an approved deviation record ([deviation process](../../08_process/misra/deviation_process.md)).
5. In-code suppressions use only the single-line form `/*cstat !<tag> : DEV-DCU-nnn <reason>*/`.

### 4.8 Debugger (C-SPY with the on-board ST-LINK)

| Page | Debug | Hil | Release |
|---|---|---|---|
| Debugger > Setup | Driver ST-LINK; run to `main`; setup macro `$PROJ_DIR$\cspy\dcu_debug.mac` | Driver ST-LINK; run to `main`; no setup macro | As Hil |
| Debugger > Download | Verify download; use the default flash loader | Same | Same |
| Debugger > Plugins | Visual State C-SPYLink plugin on | Off | Off |
| ST-LINK > Setup | Interface SWD; reset "System" (use "Connect during reset" if the target cannot be halted) | Same | Same |
| ST-LINK > Communication | USB, first connected probe; no probe serial number in the committed project | Same | Same |

- `dcu_debug.mac` sets `DBG_IWDG_STOP` in DBGMCU_CR at every reset, so the IWDG does not reset the MCU at a breakpoint. It is used only in the Debug configuration; Hil and Release keep the IWDG running when halted, so a halt resets the MCU and the outputs return to the safe state.
- Bench rule: while the CPU is halted with a motor connected, the 12 V supply output is off or limited to 0.5 A. A halted CPU stops the software protections and the bridge inputs keep their state.

## 5. Acceptance of the project

| Check | Expected result |
|---|---|
| Build Debug, Hil and Release in the IDE | 0 errors, 0 warnings |
| `firmware/dcu/scripts/iar/Invoke-DcuBuild.ps1 -Configs Debug,Hil,Release -CStat` | Exit code 0; images, map files and the C-STAT report in `out/` |
| Map file | No heap symbols; CSTACK at 0x20000000; `ls_rom_crc` at 0x0801EFFC; no `Fi_` symbols in Release |
| Post-build output | `dcu.out` and `dcu.hex` carry the same ROM CRC; the CI ROM CRC check passes |
| Layer isolation | A temporary file in `app` that includes `stm32f1xx.h` fails to compile (not committed) |
| C-SPY | Debug session starts on the bench NUCLEO; Hil image resets at a halt (IWDG running) |
| CI | After commit and push, `iar-gate` passes on the Lab Host runner |

## 6. What to commit

- Commit `dcu.eww`, `dcu.ewp`, `dcu.ewd`, `dcu.ewt` and `cspy/dcu_debug.mac` from the Lab Host clone, with Conventional Commits scope `dcu` or `build`.
- Never commit `settings/`, configuration output directories, `*.dep` or C-STAT databases; `.gitignore` excludes them.
- Review the XML diff of `.ewp` changes in the pull request: every option change is stated in the pull-request description.

## 7. Using the project from the workstation

The primary daily flow uses VS Code on the Mac connected to the Lab Host clone with Remote-SSH (Lab Host check D10).

1. Connect VS Code to host `labhost` and open `C:\dev\locksys`.
2. Install the "IAR Build" and "IAR C-SPY Debug" extensions on the remote side (both are in `.vscode/extensions.json`) and trust the workspace.
3. In the IAR Build view, select the EWARM installation (the directory in `LS_EWARM_DIR` of the runner set-up), the workspace `firmware/dcu/iar/dcu.eww`, the project `dcu` and the configuration.
4. Build, rebuild and run C-STAT from the extension; the extension uses the settings stored in the project, so results equal IDE and CI builds.
5. Debug with the IAR C-SPY Debug extension; it uses the debugger settings of `dcu.ewd` and the ST-LINK connected to the Lab Host.
6. Project option changes are made in the IDE over remote desktop (§4), then committed.

Command-line equivalents on the Lab Host (PowerShell):

```powershell
firmware/dcu/scripts/iar/Invoke-DcuBuild.ps1 -Configs Debug,Hil,Release -CStat
```

The script detects the EWARM line: on 9.70.x it uses `iarbuild dcu.ewp -build <cfg>`, `-cstat_analyze <cfg>` and `-cstat_report <cfg>`; on 10.10.x it uses `iarbuild -build`, a compilation database and `iarbuild -E cstat_analyze` (or `-cstat_cmds` with `icstat` and `ireport`).

## 8. Rationale

- **Group-level CMSIS include paths.** Register access outside platform, MCAL and startup becomes a compile error, which enforces the layer rule without a separate tool.
- **Checksum and hex conversion in one post-build script.** The released image is always the checksummed one, independent of the order in which the IDE runs its output converter.
- **Identical optimisation for Hil and Release.** Timing evidence measured on the Hil image applies to the released image.

## 9. References

- [LS-DCU-SAD-001 DCU software architecture](architecture.md)
- [Guideline Enforcement Plan](../../08_process/misra/gep.md), [deviation process](../../08_process/misra/deviation_process.md)
- [Lab Host](../../08_process/lab_host.md), [CI/CD](../../08_process/ci_cd.md), [Toolchains](../../08_process/toolchains.md)
- [ADR 0012: Mac workstation with Windows Lab Host for IAR](../../adr/0012-mac-workstation-with-windows-lab-host-for-iar.md)
- IAR C/C++ Development Guide for Arm (ielftool checksum, stack usage analysis, linker configuration); IAR Embedded Workbench IDE Project Management and Building Guide (iarbuild, argument variables); IAR C-STAT Static Analysis Guide
