#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# Run Ceedling projects in the CI container and apply the coverage gate.
#
# Usage: tools/ci/ceedling_ci.sh <coverage-component> <project-dir>...
#
# A project that enables the gcov plugin runs "gcov:all" and its gcovr JSON
# report is checked by tools/ci/coverage_gate.py against the thresholds of
# <coverage-component> in tools/ci/quality_gates.yaml; other projects run
# "test:all" only.
set -euo pipefail

if [ "$#" -lt 2 ]; then
  echo "usage: $0 <coverage-component> <project-dir>..." >&2
  exit 2
fi

component="$1"
shift
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

gate_args=()
for project in "$@"; do
  if [ ! -f "$project/project.yml" ]; then
    echo "error: $project/project.yml not found" >&2
    exit 2
  fi
  task="test:all"
  if grep -Eq '^[[:space:]]*-[[:space:]]*gcov[[:space:]]*$' "$project/project.yml"; then
    task="gcov:all"
    gate_args+=(--ceedling-project "$project")
  else
    echo "::warning title=coverage::$project does not enable the gcov plugin; coverage is not measured"
  fi
  echo "::group::Ceedling $task in $project"
  # clobber first: gcov counters accumulate across runs in an existing build directory.
  tools/docker/ceedling/run.sh "$project" clobber "$task"
  echo "::endgroup::"
done

if [ "${#gate_args[@]}" -gt 0 ]; then
  summary_args=()
  if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
    summary_args=(--summary "$GITHUB_STEP_SUMMARY")
  fi
  uv run tools/ci/coverage_gate.py --component "$component" "${gate_args[@]}" ${summary_args[@]+"${summary_args[@]}"}
fi
