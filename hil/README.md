# LockSys HIL framework

| Field | Value |
|---|---|
| Package | `locksys-hil` (uv workspace member; lock file `uv.lock` at the repository root) |
| Architecture | LS-HIL-001 (`docs/07_verification/hil_architecture.md`) |
| Test catalogue | LS-HIL-002 (`docs/07_verification/hil_test_catalog.md`) |
| Owner | jlurg |

## Purpose

Python framework and test suites for the hardware-in-the-loop bench of LockSys stage A: instrument drivers, protocol implementations shared with the CGW simulator, bench services (lock, safe state, self-test), the pytest plugin and the catalogue tests.

## Layout

| Path | Content |
|---|---|
| `src/locksys_hil/instruments/` | Abstract `Psu`, `Dmm`, `Scope`, `LogicAnalyzer`, `CanBus`, `Stimulus`; VISA/SCPI transport; in-memory fakes |
| `src/locksys_hil/protocols/` | E2E reference (`e2e`), CAN codec on the DBC (`com`), restbus, UDS-lite client, UART telemetry parser, APP simulator (`appsim`: crypto, frame codec, client, behaviours) |
| `src/locksys_hil/dut/` | Image SHA-256 check, DCU (STM32CubeProgrammer CLI) and CGW (esptool API) flashing |
| `src/locksys_hil/bench/` | `Bench` facade, `BenchLock`, `safe_state()`, pre-test self-test |
| `src/locksys_hil/analysis/` | Edge extraction, VNH5019 drive decoding, timing statistics and the guard-band rule |
| `src/locksys_hil/security/` | Console redaction and evidence secret scan |
| `src/locksys_hil/reporting/` | Run manifest, evidence layout, identifier registry, `trace.json` |
| `src/locksys_hil/pytest_plugin/` | Markers, collection checks, JUnit properties, `bench`/`topology`/`safe_bench` fixtures, exit code 10 |
| `src/locksys_hil/gen/` | Generated from `interfaces/` by `tools/codegen/regen.py` (never edited) |
| `config/` | Bench YAML files, JSON schema, logic-analyser profiles |
| `tests/` | Catalogue tests by topology (T1, T2, T3) |
| `unit/` | Hardware-free framework tests (run by the CI `python` job) |
| `procedures/` | Manual procedure bodies (TST-MAN-SYS-nnn) |
| `qualification/` | Bench qualification records and negative controls |
| `hardware/` | Wiring tables |
| `stimulus_fw/` | Stimulus MCU firmware (M5) |

## Usage

From the repository root:

```sh
uv sync                                              # workspace environment
uv run pytest hil/unit                               # framework tests, no hardware
uv run pytest hil/tests -m smoke                     # skipped without a bench configuration
uv run pytest hil/tests -m "smoke and hil_sim" --hil-bench hil/config/benches/lab-win-01.yaml
uv run hil config validate hil/config/benches/*.yaml
uv run hil config schema --check hil/config/schema/bench.schema.json
uv run hil --bench hil/config/benches/lab-win-01.yaml safe-state
uv run hil telemetry check vcp.log
```

The bench configuration is taken from `--hil-bench` (pytest) or `--bench` (CLI), or from the environment variable `LOCKSYS_HIL_BENCH`. Without it every bench test is skipped.

## Rules

- Catalogue tests carry exactly one `test_id`, at least one `verifies`, `hil_sim` or `hil_real`, a `topology` marker and their suite markers; the plugin rejects the run otherwise and checks every identifier against the requirement tables in `docs/`.
- Infrastructure errors (`BenchInfrastructureError`: instrument time-out, enumeration, bench busy, self-test) are reported as errors and end the session with exit code 10. Functional failures are never retried.
- The framework bench lock is the file named by `lock_file` in the bench YAML. It is not the manual reservation file `C:\labhost\bench.lock`, which the runner account can only read.
- Secrets (K_pair, client ID, Wi-Fi passphrase) live in the OS keyring under the names in the bench YAML. Every console line passes through `ConsoleRedactor`; pytest never runs with `--showlocals`; `hil evidence scan-secrets` runs before any upload.
- `safe_state()` switches the actuator supply off and releases all stimulus outputs; it runs in fixture teardown and at interpreter exit.
