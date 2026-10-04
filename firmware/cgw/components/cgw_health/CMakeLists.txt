# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# cgw_health (core): CGW mode, DTC table and reset-reason mapping.

idf_component_register(
    SRCS src/cgw_health.c
    INCLUDE_DIRS include
    REQUIRES ls_common
)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/cgw_component.cmake)
cgw_component_warnings(core)
