# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# cgw_proto (core): APP protocol 1.0 codec over the generated nanopb code in gen/ (DO NOT EDIT).

idf_component_register(
    SRCS src/cgw_proto.c gen/locksys_app.pb.c
    INCLUDE_DIRS include gen
    REQUIRES cgw_ports nanopb
)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/cgw_component.cmake)
cgw_component_warnings(core)
