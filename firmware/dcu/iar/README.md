# IAR workspace and project

The IAR workspace `dcu.eww` and project `dcu.ewp` of the DCU are created in the EWARM IDE on the
Lab Host as described in the [IAR project setup guide](../../../docs/04_software/dcu/iar_project_setup.md)
(LS-DCU-GDE-002). IAR project files are never written or edited by hand.

| Commit | Do not commit |
|---|---|
| `dcu.eww`, `dcu.ewp` | `settings/` |
| `dcu.ewd` (debugger settings) | `Debug/`, `Hil/`, `Release/` (build outputs) |
| `dcu.ewt` (C-STAT and C-RUN selection) | `*.dep`, C-STAT databases |
| `cspy/dcu_debug.mac` | `*/gen/ls_build_info_gen.h` (generated at build time) |

Project settings that reference files of this repository:

| Setting | Value |
|---|---|
| Linker configuration | `$PROJ_DIR$\..\linker\dcu_stm32f103rb.icf` |
| Stack usage control file | `$PROJ_DIR$\..\linker\dcu.suc` |
| Start-up file (group `startup`) | `$PROJ_DIR$\..\startup\startup_stm32f103xb_iar.s` |
| Pre-build action | `scripts\iar\New-BuildInfo.ps1` (version header) |
| Post-build action | `scripts\iar\Invoke-PostLink.ps1` (ROM fill, ROM CRC, HEX) |
| C-SPY setup macro (Debug) | `$PROJ_DIR$\cspy\dcu_debug.mac` |

Command-line build and C-STAT: `firmware/dcu/scripts/iar/Invoke-DcuBuild.ps1`. While
`dcu.ewp` does not exist, the `iar-gate` check passes with a warning.
