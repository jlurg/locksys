# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# cgw_com (core) [SAF]: can_io logic over the generated cantools code in gen/ (DO NOT EDIT).

idf_component_register(
    SRCS src/cgw_com.c gen/locksys_cgw.c
    INCLUDE_DIRS include gen
    REQUIRES cgw_ports ls_e2e ls_common
)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/cgw_component.cmake)
cgw_component_warnings(core)
