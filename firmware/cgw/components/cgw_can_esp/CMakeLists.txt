# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# cgw_can_esp (adapter): TWAI node lifecycle, IRAM callbacks, timing, filter, TX slots, CAN port.

idf_component_register(
    SRCS src/cgw_can_esp.c
    INCLUDE_DIRS include
    REQUIRES cgw_ports freertos
    PRIV_REQUIRES esp_driver_twai esp_driver_gpio ls_common log
)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/cgw_component.cmake)
cgw_component_warnings(adapter)
