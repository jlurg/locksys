# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# cgw_ws_esp (adapter): HTTP server, /ws/v1 WebSocket endpoint, link port.

idf_component_register(
    SRCS src/cgw_ws_esp.c
    INCLUDE_DIRS include
    REQUIRES cgw_ports
    PRIV_REQUIRES esp_http_server lwip log
)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/cgw_component.cmake)
cgw_component_warnings(adapter)
