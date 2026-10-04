# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# cgw_crypto_psa (adapter, host-portable PSA API): crypto and RNG ports, start-up KATs.

idf_component_register(
    SRCS src/cgw_crypto_psa.c
    INCLUDE_DIRS include
    REQUIRES cgw_ports
    PRIV_REQUIRES mbedtls esp_hw_support
)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/cgw_component.cmake)
cgw_component_warnings(adapter)
