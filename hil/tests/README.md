# HIL catalogue tests

Catalogue tests of LS-HIL-002 (`docs/07_verification/hil_test_catalog.md`), grouped by
integration topology (LS-HIL-001 section 4):

| Directory | Topology | Under test |
|---|---|---|
| `t1_dcu/` | T1 | DCU in the loop, CGW restbus on the USB-CAN adapter |
| `t2_cgw/` | T2 | CGW in the loop, DCU restbus on the USB-CAN adapter |
| `t3_system/` | T3 | Complete system, USB-CAN adapter as listen-only monitor |

Rules:

- Every test carries exactly one `test_id`, at least one `verifies`, a configuration marker
  (`hil_sim` or `hil_real`), a `topology` marker and its suite markers (`smoke`, `regression`,
  `nightly`, `soak`). The plugin rejects the run otherwise.
- Docstrings follow Objective / Preconditions / Steps / Expected.
- Without a bench configuration (`--hil-bench` or `LOCKSYS_HIL_BENCH`) every test is skipped.
- The M0 tests are placeholders that open the bench and the topology and then skip; the
  catalogue bodies are implemented in M5.

Run the smoke suite on a bench:

```sh
uv run pytest hil/tests -m "smoke and hil_sim" --hil-bench hil/config/benches/lab-win-01.yaml
```
