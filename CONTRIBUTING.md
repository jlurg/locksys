# Contributing to LockSys

LockSys is maintained by one maintainer. During the MVP, pull requests can only be opened by
collaborators; questions and ideas are welcome in GitHub Discussions. The process documents in
[`docs/08_process/`](docs/08_process) are normative; this file summarises them.

## Ground rules

- English only in the repository: code, comments, documentation and commit messages.
- Every change goes through an issue, a work branch and a pull request. Protected branches
  (`main`, `develop`, `release/*`, `hotfix/*`) accept changes only through pull requests with
  passing required checks.
- Commits are SSH-signed and use the contributor's GitHub noreply address.
- Humans are the only authors. AI assistants may be used as tools, but they are never named as
  authors or co-authors, and no AI attribution appears in commits, pull requests, tags,
  releases, code or documentation. See the [AI policy](docs/08_process/ai_policy.md) and
  [CLAUDE.md](CLAUDE.md).
- Participation is governed by the [Code of Conduct](CODE_OF_CONDUCT.md). Security issues are
  reported privately as described in [SECURITY.md](SECURITY.md).

## Development environment

The pinned versions of every tool are in [`tools/versions.env`](tools/versions.env); the
[toolchains document](docs/08_process/toolchains.md) describes the installation.

| Tool | Used for |
|---|---|
| uv with Python 3.13 | Code generation, tools, HIL framework, linters |
| pre-commit (installed through uv) | Git hooks |
| Docker | Ceedling unit tests, ESP-IDF builds |
| CMake, Ninja, Arm GNU Toolchain 13.3.rel1 | DCU GCC shadow build |
| FVM with Flutter 3.47.6 | APP |
| IAR Embedded Workbench for Arm and IAR Visual State | DCU builds and C-STAT on the Lab Host ([Lab Host](docs/08_process/lab_host.md)) |

Set up a clone:

```bash
uv sync
uv run pre-commit install --install-hooks
```

The hooks run formatters and linters on every commit and check every commit message for the
Conventional Commits format and for AI attribution.

## Workflow

1. Open or pick an issue. Feature work needs approved requirement IDs; see the Definition of
   Ready in [commits and pull requests](docs/08_process/commits_and_prs.md).
2. Create a work branch from the current `origin/develop`:

   ```bash
   tools/git/new-branch.sh feature 42 win-ctrl-model   # creates feature/42-win-ctrl-model
   ```

   Branch names follow the [branching model](docs/08_process/branching.md):
   `<type>/<issue>-<slug>` with type `feature`, `fix`, `docs`, `ci`, `build`, `refactor`,
   `test`, `chore` or `perf`. `tools/git/conventions.py branch <name>` checks a name.
3. Commit with Conventional Commits:

   ```text
   <type>(<scope>[,<scope>...])[!]: <subject>
   ```

   Types: `feat`, `fix`, `perf`, `refactor`, `test`, `docs`, `build`, `ci`, `chore`,
   `revert`. Scopes: `dcu`, `cgw`, `app`, `hil`, `can`, `proto`, `libs`, `docs`, `ci`,
   `build`, `tools`, `deps`, `deps-dev`, `release`, `repo`. The header has at most 72
   characters; the subject starts with a lowercase letter or digit and has no trailing
   period. Footers: `Closes #<issue>` or `Refs #<issue>`, `Req: <IDs>`, and
   `Co-authored-by:` for human co-authors only.
4. Open a pull request into `develop` using the template. The title follows the same format
   and becomes the squash commit subject; the body becomes the commit body. Link the issue.
5. Required checks: `pr-policy` and `ci-gate` on every pull request, and `iar-gate` from the
   Lab Host for changes that reach the DCU build. Safety-relevant changes need HIL evidence
   pasted into the pull request.
6. The maintainer merges work branches with squash and release, hotfix and back-merge
   branches with a merge commit (`tools/git/pr-merge.sh` selects the method).

## Code and documentation standards

- C code follows the [coding standard](docs/08_process/coding_standard.md) and MISRA C:2012
  with the deviation process in [`docs/08_process/misra/`](docs/08_process/misra). DCU
  firmware uses no ST HAL or LL, no heap and the layering MCAL, ECUAL, Services, RTE, SWC.
- Public APIs carry Doxygen comments. Other comments state intent only where it is not
  obvious: errata references, deviation IDs and requirement tags such as
  `/* @satisfies SWR-DCU-012 */`.
- Authored source files start with an SPDX license header, for example
  `# SPDX-License-Identifier: Apache-2.0` followed by `# Copyright (c) 2026 jlurg`.
- Generated code is regenerated from `interfaces/` with `uv run tools/codegen/regen.py` and
  committed in the same pull request; it is never edited by hand. Vendored code in
  `third_party/` is not modified.
- Documentation is Markdown in `docs/`, written as engineering documents with a header table,
  purpose and scope, normative content, rationale and references. Decisions are recorded as
  [ADRs](docs/adr/README.md).

## Canonical commands

| Purpose | Command |
|---|---|
| All hooks and linters | `uv run pre-commit run --all-files` |
| Code generation | `uv run tools/codegen/regen.py` (`--check` reports drift) |
| C unit tests | `tools/docker/ceedling/run.sh <dir-with-project.yml> test:all` |
| DCU GCC shadow build | `cmake -S firmware/dcu -B build/dcu-gcc -G Ninja -DCMAKE_TOOLCHAIN_FILE=firmware/dcu/cmake/arm-none-eabi-gcc.cmake && cmake --build build/dcu-gcc` |
| CGW build | `docker run --rm -v "$PWD":/project -w /project/firmware/cgw espressif/idf:v5.5.5 idf.py build` |
| Python tests and lint | `uv run pytest` and `uv run ruff check . && uv run mypy` |
| APP | `cd app && fvm flutter analyze && fvm flutter test` |
| Traceability report | `uv run tools/trace/trace.py --report` |
| IAR build (Lab Host) | `firmware/dcu/scripts/iar/Invoke-DcuBuild.ps1 -Configs Debug,Release -CStat` |

## License

LockSys is licensed under the [Apache License, Version 2.0](LICENSE). By contributing, you
agree that your contributions are licensed under the same terms, as stated in section 5 of
the license.
