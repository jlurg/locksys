---
status: accepted
date: 2026-10-03
decision-makers: jlurg
---

# Record architecture decisions with MADR

## Context and Problem Statement

LockSys spans four sub-projects (DCU, CGW, APP, HIL), shared interfaces and a repository governance model. Decisions on toolchain pins, branch model, runner trust model, licence and design patterns have long-lived consequences. They must be recorded with their context, the options considered and the consequences, reviewed like code, and versioned with the code they affect. How are these decisions recorded?

## Decision Drivers

- Decisions live in the repository and are reviewed in pull requests.
- Low overhead for a single maintainer.
- A fixed structure that records options and consequences, not only the outcome.
- Readable on GitHub without a build step, and convertible to Sphinx later.
- A way to state how compliance with a decision is confirmed.

## Considered Options

- Markdown Architectural Decision Records (MADR) 4.0.0
- ADRs in the original free-form format (context, decision, status, consequences)
- Decisions recorded only in design documents and PR descriptions

## Decision Outcome

Chosen option: "MADR 4.0.0", because it is a maintained, structured Markdown format that records considered options, consequences and a confirmation method, and renders directly on GitHub.

Rules:

- One file per decision in `docs/adr/`, named `NNNN-title-with-dashes.md`, numbered sequentially from `0001`.
- Front matter with `status`, `date` and `decision-makers`. Status values: `proposed`, `accepted`, `rejected`, `deprecated`, `superseded by ADR-NNNN`.
- An accepted ADR is not rewritten. A changed decision gets a new ADR that supersedes the old one; the old ADR only receives the status change and a link.
- A new ADR is required for: changes to the IAR, Visual State or ESP-IDF pins and to vendored libraries; changes to required checks, rulesets or the runner trust model; the licence; interface minor changes on a release branch; and any exception to a design rule that applies to more than one place.
- The index of ADRs is [docs/adr/README.md](README.md); new ADRs start from [the template](adr-template.md).

### Consequences

- Good, because the reasons for each decision stay next to the code and survive personnel and tool changes.
- Good, because reviewers can check new changes against recorded decisions.
- Bad, because each significant decision costs a short document; this is limited by the list of decisions that require an ADR.

### Confirmation

The PR review checklist contains "documentation, ADR and changelog impact handled". The ADR index is updated in the same PR as each new ADR.

## Pros and Cons of the Options

### Markdown Architectural Decision Records (MADR) 4.0.0

- Good, because the template records drivers, options, outcome, consequences and confirmation.
- Good, because it is plain Markdown with optional front matter.
- Neutral, because optional sections can be omitted for small decisions.

### ADRs in the original free-form format

- Good, because it is short.
- Bad, because considered options and their trade-offs are not recorded in a fixed place.

### Decisions recorded only in design documents and PR descriptions

- Good, because no extra files are needed.
- Bad, because decisions are scattered and hard to find, and PR descriptions are not versioned with the code.

## More Information

- [MADR 4.0.0](https://github.com/adr/madr/releases/tag/4.0.0)
- [Documentation index](../README.md)
