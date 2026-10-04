# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# cgw_vstate (core): vehicle-state cache, StatusUpdate builder and push policy.

idf_component_register(
    SRCS src/cgw_vstate.c
    INCLUDE_DIRS include
    REQUIRES cgw_ports cgw_proto cgw_com ls_common
)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/cgw_component.cmake)
cgw_component_warnings(core)
