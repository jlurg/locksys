# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# Nayuki qrcodegen 1.8.0 (C), vendored in third_party/ (ADR 0009). Used with static buffers only.

set(QRCODEGEN_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../../third_party/qrcodegen-1.8.0/c)

idf_component_register(
    SRCS ${QRCODEGEN_DIR}/qrcodegen.c
    INCLUDE_DIRS ${QRCODEGEN_DIR}
)
