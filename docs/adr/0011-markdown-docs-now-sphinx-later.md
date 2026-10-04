---
status: accepted
date: 2026-10-03
decision-makers: jlurg
---

# Markdown docs now, Sphinx later

## Context and Problem Statement

The project documents requirements, architecture, interfaces, safety and security concepts, verification and processes, with traceability from stakeholder needs to code and tests. The documents must be reviewable in pull requests and readable on GitHub. Sphinx with sphinx-needs and MyST offers typed requirement objects, link checks and a published site, but adds a toolchain and configuration to maintain. A trace tool that reads requirement IDs and code tags is needed in either case. Which documentation system is used in the MVP?

## Decision Drivers

- Readable on GitHub without a build step, including diagrams.
- Reviewable as plain text in pull requests.
- Traceability between requirement IDs, code tags and test results.
- Low maintenance for a single maintainer.
- A path to a published, cross-linked documentation site.

## Considered Options

- Markdown (MyST-compatible) now, Sphinx with sphinx-needs later
- Sphinx with sphinx-needs and MyST now
- MkDocs Material
- StrictDoc

## Decision Outcome

Chosen option: "Markdown (MyST-compatible) now, Sphinx with sphinx-needs later", because it gives readable, reviewable documents immediately, and keeping them MyST-compatible preserves the migration path.

- Documents are Markdown files in `docs/`, rendered by GitHub, with Mermaid diagrams. Each document starts with a header table (document ID, version, status, owner); conventions are in the [documentation index](../README.md).
- Requirement IDs are written in the documents in their canonical form; code and tests carry `@satisfies` and `@verifies` tags. `tools/trace/trace.py` builds the trace matrix from documents, tags and test results.
- The `docs` CI job runs Markdown lint, a relative-link check and the trace report.
- Sphinx with sphinx-needs and a published site is planned for milestone M6 as a stretch goal, or later; it requires a new ADR.

### Consequences

- Good, because documents need no build step and render where the code is reviewed.
- Good, because Markdown stays portable to MyST and Sphinx.
- Bad, because there are no typed requirement objects: the trace tool depends on consistent ID formatting.
- Bad, because a later migration costs effort.

### Confirmation

CI fails on Markdown lint errors (job `lint`) and broken relative links (job `docs`). The trace report runs on every change to documents or traced sources.

## Pros and Cons of the Options

### Markdown now, Sphinx later

- Good, because it has no toolchain cost in the MVP.
- Neutral, because the trace tool must parse Markdown conventions.

### Sphinx with sphinx-needs now

- Good, because requirements become typed objects with checked links and an exportable model.
- Bad, because the toolchain and configuration must be maintained from the start.

### MkDocs Material

- Good, because it produces a readable site with little configuration.
- Bad, because it has no native requirement model, and the project announced maintenance mode in November 2025.

### StrictDoc

- Good, because it has a strong requirement model and export formats.
- Bad, because it adds a second document toolchain next to plain Markdown.

## More Information

- [Documentation index](../README.md)
- [MyST Markdown](https://mystmd.org/)
- [sphinx-needs](https://sphinx-needs.readthedocs.io/)
