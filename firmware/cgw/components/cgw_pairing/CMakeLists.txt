# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# cgw_pairing (core): pairing window, pending K_pair, pairing payload and SoftAP credentials [SEC].

idf_component_register(
    SRCS src/cgw_pairing.c
    INCLUDE_DIRS include
    REQUIRES cgw_ports ls_common
)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/cgw_component.cmake)
cgw_component_warnings(core)
