# C-STAT check notes

| Field | Value |
|---|---|
| Owner | jlurg |
| Scope | Gating rule set of the DCU (`Hil` and `Release` configurations) |

## Enabled checks

- Packages: MISRA C:2023 package for MISRA C:2012 Amendments 1 to 4 (Mandatory, Required and the
  adopted Advisory guidelines of the GEP), the CERT C subset of the GEP and the C-STAT standard
  checks. Every MISRA package of C-STAT covers selected rules only; the guidelines without a
  C-STAT check are covered as listed in the
  [DCU MISRA compliance record](../../../docs/04_software/dcu/misra_compliance.md).
- EWARM 9.70.x: the selection is stored in `firmware/dcu/iar/dcu.ewt`; the enabled-check list
  is exported to `enabled_checks.txt` in this directory for review.
- EWARM 10.10.x: the selection is `cstat_config.yaml`, and `iarbuild -cstat_checks` lists the
  enabled checks.

## Review of a change

1. A change of the selection or of `permit_scopes.yaml` is a pull request with scope `dcu`.
2. The pull request lists the added and removed check tags and the reason, with a reference to
   the GEP section.
3. A permit scope is widened only together with the permit record in the deviation register.

## Path-scoped findings

| Path | Treatment |
|---|---|
| `firmware/dcu/src/mcal/`, `firmware/dcu/startup/`, the SafeMon ROM checksum pointer | DP-01 for peripheral base address casts and the ROM checksum range |
| `firmware/dcu/src/platform/ls_compiler.h` | DP-02 for language extensions |
| `ls_compiler.h`, `ls_static_assert.h`, `det.h`, MCAL register-instance macros | DP-04 for function-like macros |
| `firmware/dcu/gen/`, `libs/ls_common/gen/` | DP-03; findings fixed in the generator |
| `firmware/dcu/gen_vs/release/` | DP-05; guideline list fixed after the first baseline |
| `third_party/` | Not analysed as project code |
