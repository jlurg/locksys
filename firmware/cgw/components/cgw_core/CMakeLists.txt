# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# cgw_core (core): core active object; dispatch and time event over the portable modules.

idf_component_register(
    SRCS src/cgw_core.c src/cgw_core_app.c
    INCLUDE_DIRS include
    REQUIRES cgw_ports cgw_proto cgw_session cgw_pairing cgw_arbiter cgw_vstate cgw_health
             cgw_com ls_e2e ls_common
)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/cgw_component.cmake)
cgw_component_warnings(core)
