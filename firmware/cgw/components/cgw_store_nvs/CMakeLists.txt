# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# cgw_store_nvs (adapter): key store port over NVS namespace cgw_sec.

idf_component_register(
    SRCS src/cgw_store_nvs.c
    INCLUDE_DIRS include
    REQUIRES cgw_ports
    PRIV_REQUIRES nvs_flash
)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/cgw_component.cmake)
cgw_component_warnings(adapter)
