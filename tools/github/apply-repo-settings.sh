#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# Apply the GitHub configuration of docs/08_process/github_settings.md to the repository.
# Idempotent: every run converges the repository to the definition files in this directory.
#
# Usage: tools/github/apply-repo-settings.sh [--dry-run] <base|labels|rulesets|all>
#   base      repository, Actions and security settings, variables, release environment
#   labels    labels (labels.tsv) and milestones (milestones.tsv)
#   rulesets  branch and tag rulesets (rulesets/*.json); run after pr-policy and ci-gate
#             have reported at least once
#   all       base, labels and rulesets
#
# Environment:
#   REPO              target repository (default: jlurg/locksys)
#   RELEASE_REVIEWER  required reviewer of the release environment (default: jlurg)
#   INCLUDE_IAR_GATE  1: add the iar-gate required check to the branch rulesets
#   INCLUDE_HIL_GATE  1: add the hil-gate required check to the main ruleset
#   DRY_RUN           1: print write requests instead of sending them (same as --dry-run)
#
# Requires gh (authenticated as a repository administrator) and jq.
set -euo pipefail

REPO="${REPO:-jlurg/locksys}"
RELEASE_REVIEWER="${RELEASE_REVIEWER:-jlurg}"
INCLUDE_IAR_GATE="${INCLUDE_IAR_GATE:-0}"
INCLUDE_HIL_GATE="${INCLUDE_HIL_GATE:-0}"
DRY_RUN="${DRY_RUN:-0}"
ACTIONS_APP_ID=15368
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
  awk '/^# Usage:/ { show = 1 } show && !/^#/ { exit } show { sub(/^# ?/, ""); print }' "${BASH_SOURCE[0]}"
  exit 2
}

die() {
  echo "error: $*" >&2
  exit 1
}

log() {
  echo "==> $*"
}

# Read-only API call (also executed in dry-run mode).
api_get() {
  gh api -H "Accept: application/vnd.github+json" -H "X-GitHub-Api-Version: 2022-11-28" "$@"
}

# Write API call: METHOD ENDPOINT [JSON-BODY].
api_write() {
  local method="$1" endpoint="$2" body="${3:-}"
  if [[ "$DRY_RUN" == "1" ]]; then
    echo "DRY-RUN $method $endpoint ${body:+$(jq -c . <<<"$body")}"
    return 0
  fi
  if [[ -n "$body" ]]; then
    api_get -X "$method" "$endpoint" --input - <<<"$body" >/dev/null
  else
    api_get -X "$method" "$endpoint" >/dev/null
  fi
}

# Run a gh command, or print it in dry-run mode.
run_gh() {
  if [[ "$DRY_RUN" == "1" ]]; then
    echo "DRY-RUN gh $*"
    return 0
  fi
  gh "$@"
}

require_tools() {
  command -v jq >/dev/null 2>&1 || die "jq is required"
  command -v gh >/dev/null 2>&1 || die "gh is required"
  if [[ "$DRY_RUN" != "1" ]]; then
    gh auth status >/dev/null 2>&1 || die "gh is not authenticated (gh auth login)"
  fi
}

base() {
  log "base settings for $REPO"
  if [[ "$DRY_RUN" != "1" ]]; then
    api_get "repos/$REPO/branches/develop" >/dev/null 2>&1 ||
      die "branch develop does not exist on $REPO; create and push it first"
  fi

  api_write PATCH "repos/$REPO" "$(jq -n '{
    default_branch: "develop",
    has_issues: true, has_projects: true, has_wiki: false,
    pull_request_creation_policy: "collaborators_only",
    allow_squash_merge: true, allow_merge_commit: true, allow_rebase_merge: false,
    allow_auto_merge: true, delete_branch_on_merge: true, allow_update_branch: true,
    squash_merge_commit_title: "PR_TITLE", squash_merge_commit_message: "PR_BODY",
    merge_commit_title: "PR_TITLE", merge_commit_message: "PR_BODY",
    web_commit_signoff_required: false
  }')"
  run_gh repo edit "$REPO" --enable-discussions

  log "Actions settings"
  api_write PUT "repos/$REPO/actions/permissions" \
    '{"enabled": true, "allowed_actions": "selected", "sha_pinning_required": true}'
  api_write PUT "repos/$REPO/actions/permissions/selected-actions" "$(jq -c . "$HERE/allowed-actions.json")"
  api_write PUT "repos/$REPO/actions/permissions/workflow" \
    '{"default_workflow_permissions": "read", "can_approve_pull_request_reviews": false}'
  api_write PUT "repos/$REPO/actions/permissions/fork-pr-contributor-approval" \
    '{"approval_policy": "all_external_contributors"}'
  api_write PUT "repos/$REPO/actions/permissions/artifact-and-log-retention" '{"days": 90}'
  run_gh variable set TRUSTED_ACTORS --repo "$REPO" --body '["jlurg"]'

  log "security features"
  local endpoint
  for endpoint in vulnerability-alerts automated-security-fixes private-vulnerability-reporting immutable-releases; do
    api_write PUT "repos/$REPO/$endpoint"
  done
  api_write PATCH "repos/$REPO" '{"security_and_analysis": {
    "secret_scanning": {"status": "enabled"},
    "secret_scanning_push_protection": {"status": "enabled"}}}'
  api_write PATCH "repos/$REPO" \
    '{"security_and_analysis": {"secret_scanning_non_provider_patterns": {"status": "enabled"}}}' ||
    echo "warning: non-provider secret patterns are not available for this repository" >&2
  api_write PATCH "repos/$REPO/code-scanning/default-setup" '{"state": "not-configured"}' ||
    echo "warning: code scanning default setup could not be changed" >&2

  release_environment

  if [[ "$DRY_RUN" != "1" ]]; then
    api_get "repos/$REPO" --jq '"default branch: \(.default_branch); PR creation policy: \(.pull_request_creation_policy // "unknown")"'
  fi
}

release_environment() {
  log "environment release (reviewer $RELEASE_REVIEWER, tag deployments v*)"
  local reviewer_id=0
  if [[ "$DRY_RUN" != "1" ]]; then
    reviewer_id="$(api_get "users/$RELEASE_REVIEWER" --jq .id)"
  fi
  api_write PUT "repos/$REPO/environments/release" "$(jq -n --argjson id "$reviewer_id" '{
    reviewers: [{type: "User", id: $id}],
    prevent_self_review: false,
    deployment_branch_policy: {protected_branches: false, custom_branch_policies: true}
  }')"
  local existing=""
  if [[ "$DRY_RUN" != "1" ]]; then
    existing="$(api_get "repos/$REPO/environments/release/deployment-branch-policies" \
      --jq '.branch_policies[] | select(.type == "tag" and .name == "v*") | .id')"
  fi
  if [[ -z "$existing" ]]; then
    api_write POST "repos/$REPO/environments/release/deployment-branch-policies" \
      '{"name": "v*", "type": "tag"}'
  fi
}

labels() {
  log "labels"
  local name color description
  while IFS=$'\t' read -r name color description; do
    [[ -z "$name" || "$name" == \#* ]] && continue
    run_gh label create "$name" --repo "$REPO" --color "$color" --description "$description" --force
  done <"$HERE/labels.tsv"

  log "milestones"
  local existing=""
  if [[ "$DRY_RUN" != "1" ]]; then
    existing="$(api_get --paginate "repos/$REPO/milestones?state=all&per_page=100" \
      --jq '.[] | "\(.number)\t\(.title)"')"
  fi
  local title due number body
  while IFS=$'\t' read -r title due description; do
    [[ -z "$title" || "$title" == \#* ]] && continue
    body="$(jq -n --arg title "$title" --arg due "$due" --arg description "$description" \
      '{title: $title, description: $description} + (if $due == "-" then {} else {due_on: $due} end)')"
    number="$(awk -F'\t' -v t="$title" '$2 == t { print $1; exit }' <<<"$existing")"
    if [[ -n "$number" ]]; then
      api_write PATCH "repos/$REPO/milestones/$number" "$body"
    else
      api_write POST "repos/$REPO/milestones" "$body"
    fi
  done <"$HERE/milestones.tsv"
}

# Print a ruleset definition with the optional gate checks added.
ruleset_body() {
  jq \
    --argjson iar "$([[ "$INCLUDE_IAR_GATE" == "1" ]] && echo true || echo false)" \
    --argjson hil "$([[ "$INCLUDE_HIL_GATE" == "1" ]] && echo true || echo false)" \
    --argjson app "$ACTIONS_APP_ID" '
    def add_check($context):
      (.rules[] | select(.type == "required_status_checks") | .parameters.required_status_checks)
        |= (. + [{context: $context, integration_id: $app}] | unique_by(.context));
    (if $iar and .target == "branch" then add_check("iar-gate") else . end)
    | (if $hil and (.conditions.ref_name.include | any(. == "refs/heads/main"))
      then add_check("hil-gate") else . end)' "$1"
}

rulesets() {
  log "rulesets (iar-gate: $INCLUDE_IAR_GATE, hil-gate: $INCLUDE_HIL_GATE)"
  local existing=""
  if [[ "$DRY_RUN" != "1" ]]; then
    existing="$(api_get --paginate "repos/$REPO/rulesets?includes_parents=false&per_page=100" \
      --jq '.[] | "\(.id)\t\(.name)"')"
  fi
  local file body name id
  for file in "$HERE"/rulesets/*.json; do
    body="$(ruleset_body "$file")"
    name="$(jq -r .name <<<"$body")"
    id="$(awk -F'\t' -v n="$name" '$2 == n { print $1; exit }' <<<"$existing")"
    if [[ -n "$id" ]]; then
      api_write PUT "repos/$REPO/rulesets/$id" "$body"
    else
      api_write POST "repos/$REPO/rulesets" "$body"
    fi
    echo "ruleset $name: $(jq -r '[.rules[] | select(.type == "required_status_checks")
      | .parameters.required_status_checks[].context] | join(", ") | if . == "" then "no required checks" else . end' <<<"$body")"
  done
}

main() {
  local command=""
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --dry-run) DRY_RUN=1 ;;
      -h | --help) usage ;;
      base | labels | rulesets | all) command="$1" ;;
      *) usage ;;
    esac
    shift
  done
  [[ -n "$command" ]] || usage
  require_tools
  case "$command" in
    base) base ;;
    labels) labels ;;
    rulesets) rulesets ;;
    all)
      base
      labels
      rulesets
      ;;
  esac
}

main "$@"
