## Summary

<!-- What changes and why, 1 to 5 lines. This text becomes the body of the squash commit. -->

## Links

Closes #
Req:
ADR:

## Impact

- [ ] Interface change (DBC, protobuf, YAML, UART telemetry). Version impact: none / minor / major (`!`)
- [ ] Safety-relevant change (WinCtrl, DoorCtrl, ModeMgr, SafeMon, WdgM, HBridge, Com/E2E, `libs/ls_e2e`, CAN matrix, CGW arbiter or session, APP hold-to-run). Affected `SM-nn`:
- [ ] Security-relevant change (keys, authentication, pairing, logs)

## Verification

- [ ] Unit tests added or updated with `@verifies` tags; coverage gate green
- [ ] Static analysis clean: C-STAT through `iar-gate` (zero open findings; every suppression cites an approved deviation), `flutter analyze`, ruff and mypy as applicable
- [ ] HIL evidence (mandatory for safety-relevant changes): workflow run link and pasted result summary
- [ ] Generated code regenerated with the generator, not edited by hand
- [ ] Documentation and traceability updated (`@satisfies`/`@verifies` tags, requirement status, trace report)

<!-- HIL result summary (workflow runs are deleted after 90 days): -->

## Hygiene

- [ ] Branch name and title follow `docs/08_process/branching.md` and `docs/08_process/commits_and_prs.md`
- [ ] No secrets, keys, passphrases or pairing URIs in code, logs or artefacts
- [ ] No AI attribution: no AI co-author or session trailers, no "Generated with" lines, no AI identities

<!-- Mark items that do not apply as N/A with a reason; do not delete them. -->
