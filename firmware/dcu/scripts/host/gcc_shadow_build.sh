#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# DCU GCC shadow build (Hil and Release variants) with the Arm GNU Toolchain.
#
# Usage: firmware/dcu/scripts/host/gcc_shadow_build.sh [build-dir]
#   build-dir   default: build/dcu-gcc (relative to the repository root)
# The compiler is found on PATH, else in ARM_GCC_BIN, else in ARM_GCC_MAC_BIN of
# tools/versions.env. Ninja is used when available (also from STM32CubeCLT), else Make.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
cd "$repo_root"
build_dir="${1:-build/dcu-gcc}"

if ! command -v arm-none-eabi-gcc > /dev/null 2>&1 && [ -z "${ARM_GCC_BIN:-}" ]; then
  mac_bin="$(sed -n 's/^ARM_GCC_MAC_BIN=//p' tools/versions.env)"
  if [ -n "$mac_bin" ] && [ -x "$mac_bin/arm-none-eabi-gcc" ]; then
    export ARM_GCC_BIN="$mac_bin"
  fi
fi

generator=()
if command -v ninja > /dev/null 2>&1; then
  generator=(-G Ninja)
else
  for candidate in /opt/ST/STM32CubeCLT_*/Ninja/bin/ninja; do
    if [ -x "$candidate" ]; then
      generator=(-G Ninja "-DCMAKE_MAKE_PROGRAM=$candidate")
      break
    fi
  done
fi

cmake -S firmware/dcu -B "$build_dir" "${generator[@]}" \
  -DCMAKE_TOOLCHAIN_FILE="$repo_root/firmware/dcu/cmake/arm-none-eabi-gcc.cmake"
cmake --build "$build_dir"
