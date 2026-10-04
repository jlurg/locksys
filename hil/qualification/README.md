# Bench qualification

Records of the test-environment qualification of LS-HIL-001 section 11.

| Record | Content |
|---|---|
| `instruments.yaml` | Instrument register: identity, verification method, interval, due date |
| `bqr/BQR-<bench>-rev<X>.md` | Bench qualification report per bench hardware revision; header QUALIFIED or NOT QUALIFIED |
| `negative_controls/` | Weekly negative-control runs |

## Negative controls

A negative control flashes a DEV build with a seeded defect (`!LSFI,SEED,n`) and runs the named test, which must fail. A negative control that passes is a bench defect: the bench becomes NOT QUALIFIED.

| Seed | Seeded fault | Test that must fail |
|---|---|---|
| N1 | DCU `WinCmd` RX timeout disabled | TST-HIL-SYS-004 |
| N2 | Encoder supervision (`NO_MOTION`) disabled | TST-HIL-SYS-007 |
| N3 | E2E CRC check bypassed | TST-HIL-SYS-010 |
| N4 | Temperature rounding error of 0.01 °C | TST-HIL-SYS-017 |

## Rules

- The release regression refuses to run on a NOT QUALIFIED bench.
- A BQR is repeated after any change of wiring, instrument or DUT board.
- Records never contain secrets; DUT identities are serial numbers and MAC addresses only.
