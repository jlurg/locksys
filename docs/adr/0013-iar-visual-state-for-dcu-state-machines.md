---
status: accepted
date: 2026-10-03
decision-makers: jlurg
---

# IAR Visual State for DCU state machines (Classic Coder, readable code)

## Context and Problem Statement

Three DCU software components are sequencing state machines: WinCtrl (hold-to-run window movement with brake, dead time, stall and fault handling), DoorCtrl (lock pulses with verification, retries and rate limits) and ModeMgr (INIT, NORMAL, DEGRADED and SAFE). The DCU design rules forbid heap, recursion and function pointers, and the code must comply with MISRA C:2012 and be unit-tested on the host. The maintainer's IAR licence includes IAR Visual State, which offers modelling, formal verification (Verificator), simulation (Validator), documentation and on-target animation (C-SPYLink), and generates C code. Relevant tool facts:

- Only the Classic Coder produces readable code; the Hierarchical Coder, the default for new projects, dispatches through tables of function pointers.
- The Adaptive API with readable output generates `switch` and `if` code with direct calls to the actions.
- Generated code uses dynamic memory unless `-useheap0` is set.
- The readable `VSDeduct` function is declared variadic.
- Visual State runs on Windows and Linux hosts, not on macOS.

Should these state machines be modelled in Visual State, and how is the generated code integrated?

## Decision Drivers

- Static verification and validation evidence for safety-relevant sequencing logic.
- Generated code that respects the design rules (no heap, no recursion, no function pointers) and MISRA C:2012.
- Exact stack analysis and readable code that can be reviewed next to the model.
- Builds, unit tests and static analysis without a Visual State licence.
- Safety independent of the correctness of the model or the generator.

## Considered Options

- Hand-written state machines (enumeration and `switch`)
- IAR Visual State with the Hierarchical Coder
- IAR Visual State with the Classic Coder, Adaptive API and readable output
- A different statechart tool

## Decision Outcome

Chosen option: "IAR Visual State with the Classic Coder, Adaptive API and readable output", because it provides model verification and validation while generating direct-call C code without heap or function pointers, which can be analysed by C-STAT and tested on the host.

Rules:

- One project `dcu_fsm.vsp` with three systems: WinCtrl, DoorCtrl and ModeMgr. Low-level state machines (I2C recovery, CAN bus-off, E2E reception) stay hand-written.
- Coder options are kept in committed option files in `firmware/dcu/model/visualstate/options/` and include `-api_type0 -readable1 -useheap0 -maximummisra1 -typestyle1 -D2 -generatetimeandversion0 -warnings_are_errors1` and an API prefix per system (for example `WinCtrlVSDeduct`). The generator is never started from the GUI button.
- Generation runs only through `firmware/dcu/scripts/vs/Invoke-VsGenerate.ps1` into `firmware/dcu/gen_vs/release/` and `firmware/dcu/gen_vs/debug/`. Generated code is committed and never edited.
- Each component has a hand-written adapter called in the 10 ms task: it reads the RTE once, evaluates timers (`LsTmr`) and guards, posts events from a priority-ordered bitset (`LsEvSet`), calls the engine once per event, and applies the buffered outputs once per cycle.
- The safety layer stays outside the model: interrupt-level reflexes, the H-bridge reflex latch, current and motion supervision, the lock pulse hard limit, the hang monitor, SafeMon and the watchdog. The model sequences; it does not protect.
- An engine error (contradiction or range error) makes the adapter switch the actuator off directly, record a DTC and enter SAFE.
- C-STAT analyses the generated release code; remaining findings are covered by the path-scoped permit DP-05 after the first baseline ([Guideline Enforcement Plan](../08_process/misra/gep.md)).
- Fallback "model as specification": if a Mandatory guideline, or a Required guideline with real risk, cannot be met by generator options, the affected component keeps the model as design and validation artefact and its C code is written by hand from it.

### Consequences

- Good, because the Verificator and Validator provide static and dynamic evidence at model level, and C-SPYLink animates the model on the target.
- Good, because readable, direct-call code keeps stack analysis exact and makes review against the model possible.
- Good, because committed generated code lets IAR builds, Ceedling tests and C-STAT run without a Visual State licence.
- Bad, because modelling requires remote desktop access to the Lab Host.
- Bad, because the generator is not qualified; acceptable because the safety layer below the model limits the hazard if the model or the generated code is wrong.
- Bad, because model files are XML with identifiers and layout data: they are merged as binary files and edited by one person at a time.
- Bad, because the engines add flash and RAM use (estimated 6 to 10 KB of flash), to be measured from the map file.

### Confirmation

- CI on GitHub-hosted runners: `uv run tools/vs/vs_manifest.py --check` detects a model changed without regeneration and generated code edited by hand.
- On the Lab Host: regeneration with the committed options followed by `git diff --exit-code firmware/dcu/gen_vs` and a Verificator report with zero critical findings.
- Unit tests run the real engines with the real actions and reach every transition.
- The first-generation check (D9 in [Lab Host](../08_process/lab_host.md)) confirms the Classic Coder output, the generated names, the use of variadic arguments, the presence of contradiction tests and the C-STAT baseline.

## Pros and Cons of the Options

### Hand-written state machines

- Good, because no additional tool is involved.
- Bad, because there is no model-level verification, simulation or animation.

### Visual State with the Hierarchical Coder

- Good, because it is the default generator.
- Bad, because it dispatches through function-pointer tables, which violates the design rules and makes stack analysis inexact.

### Visual State with the Classic Coder, Adaptive API, readable output

- Good, because it combines model verification with code that meets the design rules.
- Neutral, because one API copy per system adds some flash.

### A different statechart tool

- Bad, because it would add a toolchain and licence outside the existing IAR installation.

## More Information

- [Visual State modelling guide](../04_software/dcu/visual_state_guide.md)
- [DCU software architecture](../04_software/dcu/architecture.md)
- [Guideline Enforcement Plan](../08_process/misra/gep.md)
- [IAR Visual State](https://www.iar.com/embedded-development-tools/iar-visual-state)
