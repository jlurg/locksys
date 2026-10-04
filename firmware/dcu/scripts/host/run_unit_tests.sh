#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# Run the DCU host unit tests (Ceedling, pinned container image).
#
# Usage: firmware/dcu/scripts/host/run_unit_tests.sh [ceedling arguments...]
#   default: test:all; "gcov:all" adds the coverage report
#   (firmware/dcu/build/artifacts/gcov/gcovr/); "--gate" runs the CI form with the coverage gate.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
cd "$repo_root"

if [ "${1:-}" = "--gate" ]; then
  exec tools/ci/ceedling_ci.sh dcu firmware/dcu
fi
if [ "$#" -eq 0 ]; then
  set -- test:all
fi
exec tools/docker/ceedling/run.sh firmware/dcu "$@"
