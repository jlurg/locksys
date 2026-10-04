# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# cgw_wifi_esp (adapter): SoftAP, Wi-Fi and IP events, station table.

idf_component_register(
    SRCS src/cgw_wifi_esp.c
    INCLUDE_DIRS include
    REQUIRES cgw_ports
    PRIV_REQUIRES esp_wifi esp_netif esp_event esp_hw_support log
)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/cgw_component.cmake)
cgw_component_warnings(adapter)
