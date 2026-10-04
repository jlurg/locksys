#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# Run Ceedling for one project in the pinned container image.
#
# Usage: tools/docker/ceedling/run.sh <project-dir> [ceedling arguments...]
#   <project-dir>  directory that contains project.yml, relative to the repository root
#                  or absolute inside the repository
#   arguments      passed to ceedling unchanged (default: test:all)
#
# The image tag is CEEDLING_IMAGE from the environment, else from tools/versions.env.
# The image is built from tools/docker/ceedling/Dockerfile when it is not present locally.
set -euo pipefail

if [ "$#" -lt 1 ]; then
  echo "usage: $0 <project-dir> [ceedling arguments...]" >&2
  exit 2
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
project_arg="$1"
shift

if [ ! -d "$project_arg" ] && [ -d "$repo_root/$project_arg" ]; then
  project_arg="$repo_root/$project_arg"
fi
if [ ! -d "$project_arg" ]; then
  echo "error: project directory not found: $project_arg" >&2
  exit 2
fi
project_abs="$(cd "$project_arg" && pwd)"
case "$project_abs/" in
  "$repo_root"/*) project_rel="${project_abs#"$repo_root"}" ;;
  *)
    echo "error: $project_abs is outside the repository $repo_root" >&2
    exit 2
    ;;
esac
project_rel="${project_rel#/}"
if [ ! -f "$project_abs/project.yml" ]; then
  echo "error: $project_rel/project.yml not found" >&2
  exit 2
fi

image="${CEEDLING_IMAGE:-}"
if [ -z "$image" ]; then
  image="$(sed -n 's/^CEEDLING_IMAGE=//p' "$repo_root/tools/versions.env" | tr -d '"[:space:]')"
fi
image="${image:-locksys/ceedling:1.1.9}"

if ! docker image inspect "$image" > /dev/null 2>&1; then
  echo "Building $image from tools/docker/ceedling/Dockerfile" >&2
  docker build --tag "$image" "$repo_root/tools/docker/ceedling"
fi

if [ "$#" -eq 0 ]; then
  set -- test:all
fi

exec docker run --rm \
  --user "$(id -u):$(id -g)" \
  --volume "$repo_root":/work \
  --workdir "/work/$project_rel" \
  "$image" ceedling "$@"
