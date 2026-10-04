# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# ls_common: LsCrc8, LsEvSet, LsTmr, LsRing and the generated interface headers (gen/).
# Dual mode: ESP-IDF component (CGW, ESP_PLATFORM set by the IDF build) or a plain CMake static
# library (DCU GCC shadow build, host tools). Plain mode targets: ls_common, locksys::ls_common.

set(LS_COMMON_SOURCES
    src/ls_crc8.c
    src/ls_evset.c
    src/ls_ring.c
    src/ls_tmr.c
)

if(ESP_PLATFORM)
    idf_component_register(
        SRCS ${LS_COMMON_SOURCES}
        INCLUDE_DIRS include gen
    )
    return()
endif()

cmake_minimum_required(VERSION 3.20)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(ls_common LANGUAGES C)
endif()

option(LS_LIBS_WERROR "Treat compiler warnings in the LockSys libraries as errors" OFF)

if(NOT TARGET ls_common)
    add_library(ls_common STATIC ${LS_COMMON_SOURCES})
    add_library(locksys::ls_common ALIAS ls_common)
    target_include_directories(ls_common PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/gen
    )
    target_compile_features(ls_common PUBLIC c_std_99)
    set_target_properties(ls_common PROPERTIES C_EXTENSIONS OFF)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(ls_common PRIVATE
            -Wall -Wextra -Wconversion -Wsign-conversion -Wshadow -Wstrict-prototypes
            $<$<BOOL:${LS_LIBS_WERROR}>:-Werror>
        )
    endif()
endif()
