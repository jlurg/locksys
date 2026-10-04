#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# Create a branch that follows the LockSys branching model from the current remote state.
#
# Usage: tools/git/new-branch.sh <type> <issue> <slug> [<base>]
#   type   feature|fix|docs|ci|build|refactor|test|chore|perf|spike
#   issue  GitHub issue number
#   slug   lower-case words separated by hyphens
#   base   develop (default), release/X.Y.Z or hotfix/X.Y.Z
#
# Example: tools/git/new-branch.sh feature 42 win-ctrl-sm
#          -> feature/42-win-ctrl-sm, created from origin/develop without upstream tracking.
set -euo pipefail

usage() {
  awk '/^# Usage:/ { show = 1 } show && !/^#/ { exit } show { sub(/^# ?/, ""); print }' "${BASH_SOURCE[0]}"
  exit 2
}

[[ $# -eq 3 || $# -eq 4 ]] || usage

type="$1"
issue="$2"
slug="$3"
base="${4:-develop}"
name="${type}/${issue}-${slug}"
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if command -v python3 >/dev/null 2>&1; then
  py=(python3)
elif command -v uv >/dev/null 2>&1; then
  py=(uv run --quiet --no-project python)
else
  echo "error: python3 or uv is required" >&2
  exit 2
fi

"${py[@]}" "$here/conventions.py" branch "$name" >/dev/null
if [[ "$type" != "spike" ]]; then
  "${py[@]}" "$here/conventions.py" merge-method --base "$base" --head "$name" >/dev/null
fi

if git show-ref --verify --quiet "refs/heads/$name"; then
  echo "error: branch '$name' already exists" >&2
  exit 1
fi

git fetch --quiet origin "$base"
git switch --no-track --create "$name" "origin/$base"
echo "Created '$name' from origin/$base. Publish it with: git push -u origin HEAD"
