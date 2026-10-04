# C-STAT configuration

| Field | Value |
|---|---|
| Owner | jlurg |
| Rule set | [Guideline Enforcement Plan](../../../docs/08_process/misra/gep.md) (GEP) |
| Deviation register | [`docs/08_process/misra/deviations.yaml`](../../../docs/08_process/misra/deviations.yaml) (single register for the repository) |
| Gate | `tools/ci/cstat_gate.py`, thresholds in `tools/ci/quality_gates.yaml` |

This directory holds the C-STAT analysis configuration of the DCU, reviewed like code:

| File | Content | Source |
|---|---|---|
| `permit_scopes.yaml` | Path scopes of the permits DP-01 to DP-05 and the C-STAT checks they disable | GEP; filled at the Lab Host day-1 baseline |
| `checks_notes.md` | Notes on the enabled check list, its export and review | GEP |
| `cstat_config.yaml` | EWARM 10.10.x only: C-STAT configuration passed to `iarbuild -E cstat_analyze --cstat_config_file` | Created when the 10.10 line is licensed |
| `enabled_checks.txt` | EWARM 9.70.x: enabled-check list exported from the `dcu.ewt` selection | Lab Host day-1 export |

Rules:

- In-code suppressions use only the single-line form
  `/*cstat !<tag> : DEV-DCU-nnn <reason>*/`, and every cited record is approved in the
  deviation register. Suppressions are never placed in `gen/` or `gen_vs/`.
- Permits (DP-nn) are applied only as path scopes of the C-STAT configuration, never with
  in-code comments.
- The deviation records themselves live only in the deviation register; this directory does
  not keep a copy.
