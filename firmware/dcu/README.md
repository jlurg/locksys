# DCU firmware

Door Control Unit firmware for the STM32F103RB (NUCLEO-F103RB with the Pololu Dual VNH5019
shield #2507): hold-to-run window on bridge channel M1 (JGB37-520 encoder motor), door lock on
M2, TMP117 temperature sensor, CAN 500 kbit/s, UART telemetry on the ST-LINK virtual COM port.

| Item | Value |
|---|---|
| Architecture | [LS-DCU-SAD-001](../../docs/04_software/dcu/architecture.md) |
| Requirements | [LS-DCU-SRS-001](../../docs/04_software/dcu/software_requirements.md) |
| Contract | [LS-SAIC-001](../../docs/02_system/LS-SAIC.md) |
| Delivered image | IAR EWARM (baseline `IAR_EWARM_BASELINE` in `tools/versions.env`) on the Lab Host |
| Shadow build | Arm GNU Toolchain (`ARM_GCC`), CMake, warnings as errors |
| Unit tests | Ceedling 1.1.9 in the `locksys/ceedling` container |
| State machines | IAR Visual State engines in `gen_vs/` (no model yet: the adapters hold the outputs off) |

## Status (M0)

Buildable skeleton. Implemented: platform layer, clock tree (HSE72 with HSI64 fallback), pin
table with the single `AFIO->MAPR = 0x02000D02` write and the safe pin initialisation, interrupt
priorities, SysTick, IWDG, CRC unit, scheduler with overrun and hang supervision, start-up order,
safe switch-off of both bridges, RTE ports, SWC adapters in the Visual State adapter pattern
(compiled with `LS_CFG_VS_ENGINE_PRESENT = 0`). All other modules expose their API and return
their safe value; they are delivered in milestones M1 and M2. Reserved hooks: `DCU_WinMotion`
(`Com_SetWinMotion`), `$LSMOT` (`Tlm_Motion`), `!LSFI` (`Fi_Main10ms`, DEV and HIL builds only)
and DID 0xFD09 (`DiagHdl_ReadDid`).

## Layout

| Path | Content |
|---|---|
| `src/platform/` | Types, compiler abstraction, static assertion, critical sections |
| `src/mcal/<module>/` | Register-level drivers (Clk, Gpio, Nvic, Exti, Stk, Can, I2c, Uart, Dma, Pwm, Adc, Enc, Wdg, Crc, Pwr) |
| `src/ecual/<module>/` | HBridge, WinPos, LockAct, DigIn, TempSens, CanIf, CanTrcv, VbatMon |
| `src/services/<module>/` | Sched, TBase, Com, Dem, Dcm, IsoTp, WdgM, Tlm, EcuM, SafeMon, Det, Fi |
| `src/rte/` | RTE ports; one view `rte_<swc>.h` per SWC |
| `src/app/<swc>/` | WinCtrl, DoorCtrl, ModeMgr (Visual State adapters), CmdArb, TempMon, DiagHdl |
| `cfg/` | Build switches (`ls_cfg.h`), pin table, task table and the start-up bindings |
| `gen/` | Generated from `interfaces/` (CAN pack and unpack, DTC table); never edited |
| `gen_vs/` | Generated Visual State engines and `VS_MANIFEST.json`; never edited |
| `model/visualstate/` | Coder and Verificator option files, Validator material |
| `startup/`, `linker/` | Start-up code and linker configurations (IAR and GCC) |
| `iar/` | IAR workspace and project (created in the IDE, see [iar/README.md](iar/README.md)) |
| `cstat/` | C-STAT rule set and path-scoped permits |
| `test/` | Ceedling unit tests; `test/support/` holds the register fakes |
| `scripts/iar/`, `scripts/vs/` | Lab Host scripts (PowerShell 7) |
| `scripts/host/` | Workstation and CI scripts (bash) |

Include paths are layer-qualified (`#include "services/sched/sched.h"`); register headers are
visible only to platform, MCAL and startup sources. `tools/arch/check_layers.py` checks the
include rules of `tools/arch/layers.yaml`.

## Commands

From the repository root:

```sh
# GCC shadow build (Hil and Release variants): ELF, HEX with ROM CRC, MAP, size report
cmake -S firmware/dcu -B build/dcu-gcc -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=firmware/dcu/cmake/arm-none-eabi-gcc.cmake
cmake --build build/dcu-gcc
# or: firmware/dcu/scripts/host/gcc_shadow_build.sh

# Unit tests (coverage: gcov:all; CI form with the coverage gate: --gate)
tools/docker/ceedling/run.sh firmware/dcu test:all
firmware/dcu/scripts/host/run_unit_tests.sh --gate

# Architecture, Visual State and ROM CRC checks
uv run tools/arch/check_layers.py
uv run tools/vs/vs_manifest.py --check
uv run tools/romcrc/verify_rom_crc.py build/dcu-gcc/dcu_release.hex
```

On the Lab Host (PowerShell 7, `LS_EWARM_DIR` set):

```powershell
./firmware/dcu/scripts/iar/Test-Toolchain.ps1
./firmware/dcu/scripts/iar/Invoke-DcuBuild.ps1 -Configs Debug,Hil,Release -CStat -OutDir out
./firmware/dcu/scripts/iar/Test-CstatGate.ps1 -OutDir out
./firmware/dcu/scripts/iar/Invoke-DcuFlash.ps1 -Image out/Release/dcu.hex
./firmware/dcu/scripts/vs/Invoke-VsGenerate.ps1 [-Check]
./firmware/dcu/scripts/vs/Invoke-VsVerify.ps1
```

## Build configurations

| Switch | Debug | Hil | Release |
|---|---|---|---|
| `LS_CFG_DET`, `LS_CFG_FI`, `LS_CFG_TRACE`, `LS_CFG_TRCOV` | 1 | 1 | 0 |
| `LS_CFG_ROMCRC_ENFORCE` | 0 | 1 | 1 |
| Visual State variant | `gen_vs/debug` | `gen_vs/release` | `gen_vs/release` |

`cfg/ls_cfg.h` defaults every switch to its Release value. The GCC shadow build compiles Hil
and Release; the Release image contains no `Fi_` symbols.
