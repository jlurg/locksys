# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# ls_e2e: CAN E2E profile (LS-SAIC-001 section 7.2); depends on ls_common (LsCrc8).
# Dual mode: ESP-IDF component (CGW, ESP_PLATFORM set by the IDF build) or a plain CMake static
# library (DCU GCC shadow build, host tools). Plain mode targets: ls_e2e, locksys::ls_e2e.
# In plain mode ls_common is added from ../ls_common unless the parent project already did.

if(ESP_PLATFORM)
    idf_component_register(
        SRCS src/ls_e2e.c
        INCLUDE_DIRS include
        REQUIRES ls_common
    )
    return()
endif()

cmake_minimum_required(VERSION 3.20)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(ls_e2e LANGUAGES C)
endif()

option(LS_LIBS_WERROR "Treat compiler warnings in the LockSys libraries as errors" OFF)

if(NOT TARGET ls_common)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../ls_common ${CMAKE_CURRENT_BINARY_DIR}/ls_common)
endif()

if(NOT TARGET ls_e2e)
    add_library(ls_e2e STATIC src/ls_e2e.c)
    add_library(locksys::ls_e2e ALIAS ls_e2e)
    target_include_directories(ls_e2e PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(ls_e2e PUBLIC ls_common)
    target_compile_features(ls_e2e PUBLIC c_std_99)
    set_target_properties(ls_e2e PROPERTIES C_EXTENSIONS OFF)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(ls_e2e PRIVATE
            -Wall -Wextra -Wconversion -Wsign-conversion -Wshadow -Wstrict-prototypes
            $<$<BOOL:${LS_LIBS_WERROR}>:-Werror>
        )
    endif()
endif()
