# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# DCU GCC shadow build (LS-DCU-SAD-001 sections 14 and 15): compiles the Hil and Release
# variants of the firmware with the Arm GNU Toolchain, warnings as errors, and produces ELF,
# HEX, MAP and a size report per variant. The delivered image is built with IAR EWARM.
#
#   cmake -S firmware/dcu -B build/dcu-gcc -G Ninja \
#         -DCMAKE_TOOLCHAIN_FILE=firmware/dcu/cmake/arm-none-eabi-gcc.cmake
#   cmake --build build/dcu-gcc
#
# Register headers (CMSIS) are visible only to the platform, MCAL and startup sources.

cmake_minimum_required(VERSION 3.20)

# A relative toolchain path is taken relative to the invoking directory (the repository root in
# CI); without one, the Arm GNU Toolchain file of this directory is used.
if(DEFINED CMAKE_TOOLCHAIN_FILE AND NOT IS_ABSOLUTE "${CMAKE_TOOLCHAIN_FILE}")
    get_filename_component(LS_TOOLCHAIN_ABS "${CMAKE_TOOLCHAIN_FILE}" ABSOLUTE BASE_DIR "$ENV{PWD}")
    if(EXISTS "${LS_TOOLCHAIN_ABS}")
        set(CMAKE_TOOLCHAIN_FILE "${LS_TOOLCHAIN_ABS}" CACHE FILEPATH "Toolchain file" FORCE)
    endif()
elseif(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi-gcc.cmake"
        CACHE FILEPATH "Toolchain file")
endif()

project(locksys_dcu LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE MinSizeRel CACHE STRING "Build type" FORCE)
endif()

get_filename_component(LS_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(LS_CMSIS_CORE_INC "${LS_REPO_ROOT}/third_party/cmsis_core-5.9.0/CMSIS/Core/Include")
set(LS_CMSIS_DEVICE_INC "${LS_REPO_ROOT}/third_party/cmsis_device_f1-4.3.5/Include")
set(LS_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker/dcu_stm32f103rb.ld")

set(LS_DCU_VARIANTS Hil Release CACHE STRING "Firmware variants to build")

set(LS_DCU_WARNINGS
    -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wshadow -Wundef
    -Wstrict-prototypes -Wmissing-prototypes -Wcast-align -Wswitch-default
)

# Shared libraries (plain CMake mode of libs/ls_e2e adds libs/ls_common).
set(LS_LIBS_WERROR ON CACHE BOOL "Treat compiler warnings in the LockSys libraries as errors" FORCE)
add_subdirectory("${LS_REPO_ROOT}/libs/ls_e2e" "${CMAKE_BINARY_DIR}/libs/ls_e2e")

# Code generated from interfaces/ (CAN pack and unpack, DTC table): compiled as delivered.
file(GLOB LS_DCU_GEN_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/gen/*.c")
add_library(dcu_gen STATIC ${LS_DCU_GEN_SOURCES})
target_include_directories(dcu_gen PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/gen")
target_link_libraries(dcu_gen PUBLIC ls_common)
target_compile_options(dcu_gen PRIVATE -Wall -Wextra -Werror)

file(GLOB_RECURSE LS_DCU_HW_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mcal/*.c"
)
file(GLOB_RECURSE LS_DCU_SW_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ecual/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/services/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rte/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/app/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/cfg/*.c"
)
list(APPEND LS_DCU_SW_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")
set(LS_DCU_STARTUP "${CMAKE_CURRENT_SOURCE_DIR}/startup/startup_stm32f103xb_gcc.s")

find_program(LS_PYTHON3 python3)

foreach(variant IN LISTS LS_DCU_VARIANTS)
    string(TOLOWER "${variant}" vlow)
    if(variant STREQUAL "Hil")
        set(defs LS_CFG_DET=1 LS_CFG_FI=1 LS_CFG_TRACE=1 LS_CFG_TRCOV=1 LS_CFG_ROMCRC_ENFORCE=1)
        set(sw_sources ${LS_DCU_SW_SOURCES})
    elseif(variant STREQUAL "Release")
        set(defs LS_CFG_DET=0 LS_CFG_FI=0 LS_CFG_TRACE=0 LS_CFG_TRCOV=0 LS_CFG_ROMCRC_ENFORCE=1)
        # Release images contain no fault-injection code (SWR-DCU-102).
        set(sw_sources ${LS_DCU_SW_SOURCES})
        list(FILTER sw_sources EXCLUDE REGEX "/src/services/fi/")
    else()
        message(FATAL_ERROR "Unknown DCU variant '${variant}' (expected Hil or Release)")
    endif()
    list(APPEND defs STM32F103xB LS_CFG_VS_ENGINE_PRESENT=0)

    add_library(dcu_hw_${vlow} OBJECT ${LS_DCU_HW_SOURCES} ${LS_DCU_STARTUP})
    target_include_directories(dcu_hw_${vlow} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/cfg")
    target_include_directories(dcu_hw_${vlow} SYSTEM PRIVATE
        "${LS_CMSIS_CORE_INC}" "${LS_CMSIS_DEVICE_INC}")
    target_compile_definitions(dcu_hw_${vlow} PRIVATE ${defs})
    target_compile_options(dcu_hw_${vlow} PRIVATE $<$<COMPILE_LANGUAGE:C>:${LS_DCU_WARNINGS}>)
    target_link_libraries(dcu_hw_${vlow} PRIVATE ls_common)

    add_library(dcu_sw_${vlow} OBJECT ${sw_sources})
    target_include_directories(dcu_sw_${vlow} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/cfg")
    target_compile_definitions(dcu_sw_${vlow} PRIVATE ${defs})
    target_compile_options(dcu_sw_${vlow} PRIVATE ${LS_DCU_WARNINGS})
    target_link_libraries(dcu_sw_${vlow} PRIVATE ls_e2e ls_common dcu_gen)

    set(elf dcu_${vlow})
    add_executable(${elf} $<TARGET_OBJECTS:dcu_hw_${vlow}> $<TARGET_OBJECTS:dcu_sw_${vlow}>)
    set_target_properties(${elf} PROPERTIES SUFFIX ".elf" LINK_DEPENDS "${LS_LINKER_SCRIPT}")
    target_link_libraries(${elf} PRIVATE ls_e2e ls_common dcu_gen)
    target_link_options(${elf} PRIVATE
        "-T${LS_LINKER_SCRIPT}"
        "-Wl,-Map=$<TARGET_FILE_DIR:${elf}>/${elf}.map"
        "-Wl,--print-memory-usage"
        "-Wl,--fatal-warnings"
    )
    # The HEX carries the ROM CRC (same algorithm as the IAR post-link step) when Python is
    # available; otherwise it is a plain conversion with the 0xFFFFFFFF placeholder.
    if(LS_PYTHON3)
        set(hex_cmd "${LS_PYTHON3}" "${LS_REPO_ROOT}/tools/romcrc/verify_rom_crc.py"
            "$<TARGET_FILE:${elf}>" --write "$<TARGET_FILE_DIR:${elf}>/${elf}.hex")
    else()
        set(hex_cmd "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:${elf}>"
            "$<TARGET_FILE_DIR:${elf}>/${elf}.hex")
    endif()
    add_custom_command(TARGET ${elf} POST_BUILD
        COMMAND ${hex_cmd}
        COMMAND "${CMAKE_SIZE}" -A -x "$<TARGET_FILE:${elf}>"
        COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:${elf}>"
        VERBATIM
    )
endforeach()
