#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# Claude Code PreToolUse hook (Bash): deny commands that would write AI attribution into
# commits, tags or pull requests. The hook payload (JSON) arrives on stdin; the command text
# is checked by tools/git/ai_attribution.py --stdin. A deny decision is printed as JSON.
# The hook fails closed: when the check cannot run, the command is denied.
set -uo pipefail

deny() {
  printf '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"deny","permissionDecisionReason":"%s"}}\n' "$1"
  exit 0
}

project_dir="${CLAUDE_PROJECT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
checker="$project_dir/tools/git/ai_attribution.py"

command -v jq >/dev/null 2>&1 || deny "AI attribution check unavailable: jq not found"
payload="$(cat)"
cmd="$(jq -r '.tool_input.command // empty' <<<"$payload" 2>/dev/null)" ||
  deny "AI attribution check unavailable: invalid hook payload"
[[ -n "$cmd" ]] || exit 0

# Only commands that can create commit, tag or pull-request text are checked.
re_git='(^|[^[:alnum:]_./-])git[[:space:]].*(commit|merge|tag|notes|interpret-trailers)'
re_gh='(^|[^[:alnum:]_./-])gh[[:space:]]+(pr|release|api|issue)([[:space:]]|$)'
if [[ ! "$cmd" =~ $re_git && ! "$cmd" =~ $re_gh ]]; then
  exit 0
fi

[[ -f "$checker" ]] || deny "AI attribution check unavailable: tools/git/ai_attribution.py not found"
if command -v python3 >/dev/null 2>&1; then
  run=(python3 "$checker" --stdin)
elif command -v uv >/dev/null 2>&1; then
  run=(uv run --quiet --no-project python "$checker" --stdin)
else
  deny "AI attribution check unavailable: no Python interpreter found"
fi

printf '%s' "$cmd" | "${run[@]}" >/dev/null 2>&1
status=${PIPESTATUS[1]}
case "$status" in
  0) exit 0 ;;
  1) deny "AI attribution is forbidden in this repository (CLAUDE.md): remove AI co-author or session trailers, Generated-with lines and AI author identities" ;;
  *) deny "AI attribution check failed with exit status $status" ;;
esac
