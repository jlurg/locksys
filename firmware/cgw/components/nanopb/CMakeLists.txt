# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# nanopb 0.4.9.2 runtime, vendored in third_party/ (ADR 0009). The layout defines are PUBLIC so
# that firmware, host tests and fuzzers share the same structure layout (LS-CGW-SAD-001 3.1).

set(NANOPB_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../../third_party/nanopb-0.4.9.2)

idf_component_register(
    SRCS ${NANOPB_DIR}/pb_common.c ${NANOPB_DIR}/pb_decode.c ${NANOPB_DIR}/pb_encode.c
    INCLUDE_DIRS ${NANOPB_DIR}
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC
    PB_BUFFER_ONLY=1 PB_WITHOUT_64BIT=1 PB_MESSAGE_NESTING_MAX=4
)
# _Static_assert in pb.h requires C11 or later.
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu17)
