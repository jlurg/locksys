# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# LockSys CGW firmware (ESP32-S3, ESP-IDF v5.5.5 pinned by digest in tools/versions.env).
# Build: idf.py set-target esp32s3 build    (variants: see README.md)

cmake_minimum_required(VERSION 3.22)

set(LS_REPO_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)
set(EXTRA_COMPONENT_DIRS ${LS_REPO_ROOT}/libs/ls_e2e ${LS_REPO_ROOT}/libs/ls_common)

# PROJECT_VER follows the system version in VERSION (ADR 0006).
file(STRINGS ${LS_REPO_ROOT}/VERSION PROJECT_VER LIMIT_COUNT 1)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(locksys_cgw)
