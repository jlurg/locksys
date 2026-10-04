# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# cgw_arbiter (core) [SAF]: window press and door transaction state machines.

idf_component_register(
    SRCS src/cgw_arbiter.c
    INCLUDE_DIRS include
    REQUIRES cgw_ports ls_common
)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/cgw_component.cmake)
cgw_component_warnings(core)
