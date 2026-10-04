#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# Merge a pull request with the method the branching model requires for its base/head pair
# (squash for work, external and Dependabot branches; merge commit for release, hotfix and
# sync branches). The merge is pinned to the reviewed head commit.
#
# Usage: tools/git/pr-merge.sh [<pr-number>] [--auto]
#   pr-number  defaults to the pull request of the current branch
#   --auto     enable auto-merge once the required checks pass
set -euo pipefail

usage() {
  awk '/^# Usage:/ { show = 1 } show && !/^#/ { exit } show { sub(/^# ?/, ""); print }' "${BASH_SOURCE[0]}"
  exit 2
}

pr=""
auto=()
for arg in "$@"; do
  case "$arg" in
    --auto) auto=(--auto) ;;
    -h | --help) usage ;;
    [0-9]*) pr="$arg" ;;
    *) usage ;;
  esac
done

command -v gh >/dev/null 2>&1 || { echo "error: gh is required" >&2; exit 2; }
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if command -v python3 >/dev/null 2>&1; then
  py=(python3)
elif command -v uv >/dev/null 2>&1; then
  py=(uv run --quiet --no-project python)
else
  echo "error: python3 or uv is required" >&2
  exit 2
fi

fields="number,baseRefName,headRefName,headRefOid,state,isDraft"
template='{{.number}} {{.baseRefName}} {{.headRefName}} {{.headRefOid}} {{.state}} {{.isDraft}}'
read -r number base head sha state draft < <(gh pr view ${pr:+"$pr"} --json "$fields" --template "$template")

if [[ "$state" != "OPEN" || "$draft" == "true" ]]; then
  echo "error: pull request #$number is not an open, ready pull request (state=$state, draft=$draft)" >&2
  exit 1
fi

method="$("${py[@]}" "$here/conventions.py" merge-method --base "$base" --head "$head")"
echo "Merging #$number ($head -> $base) with --$method at ${sha:0:12}"
gh pr merge "$number" "--$method" --match-head-commit "$sha" ${auto[@]+"${auto[@]}"}
