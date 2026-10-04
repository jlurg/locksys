# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# cgw_hmi_esp (adapter): status LED (espressif/led_strip, pinned in idf_component.yml), BOOT
# button sampling and the pairing QR printer (vendored qrcodegen).

idf_component_register(
    SRCS src/cgw_hmi_esp.c
    INCLUDE_DIRS include
    REQUIRES cgw_ports
    PRIV_REQUIRES esp_driver_gpio qrcodegen mbedtls ls_common log
)

include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/cgw_component.cmake)
cgw_component_warnings(adapter)
