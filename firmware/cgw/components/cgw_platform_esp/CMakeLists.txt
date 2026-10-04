# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# cgw_platform_esp (adapter): AO runtime, clock, trace outputs, reset source, alive supervision.
# The fault-injection console exists only in DEV builds (CONFIG_CGW_FAULT_INJECTION).

# Requirements are resolved before Kconfig values exist, so console is always a requirement;
# only the source list depends on CONFIG_CGW_FAULT_INJECTION.
set(srcs src/cgw_platform_esp.c)
set(priv_requires esp_driver_gpio esp_driver_rmt esp_timer esp_hw_support esp_system mbedtls log
    console)
if(CONFIG_CGW_FAULT_INJECTION)
    list(APPEND srcs src/cgw_fi_esp.c)
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS include
    REQUIRES cgw_ports cgw_health freertos
    PRIV_REQUIRES ${priv_requires} ls_common
)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/cgw_component.cmake)
cgw_component_warnings(adapter)
