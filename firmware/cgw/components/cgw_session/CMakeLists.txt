# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# cgw_session (core) [SEC]: session table, handshake, proofs, tags, counters, rate limit.

idf_component_register(
    SRCS src/cgw_session.c
    INCLUDE_DIRS include
    REQUIRES cgw_ports cgw_proto ls_common
)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/cgw_component.cmake)
cgw_component_warnings(core)
