# Validator material

| Path | Content |
|---|---|
| `dcu_fsm.vws` | Validator workspace |
| `sequences/<ID>.vxlg` | One recorded sequence per transition identifier (W1-W19, D1-D19, M1-M23) |
| `sequences/scn_<name>.vxlg` | Scenario sequences of LS-DCU-GDE-001 section 9 |

Pass criterion: the coverage report shows 100 % of states, transitions, events and actions for
each system. Every engine unit test mirrors one sequence and carries the transition identifier
in its name. The coverage report of a release candidate is stored with the release evidence.
