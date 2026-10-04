# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# CMake toolchain file of the DCU GCC shadow build: Arm GNU Toolchain (ARM_GCC in
# tools/versions.env) for the STM32F103RB (Cortex-M3, no FPU).
# The compiler is taken from ARM_GCC_BIN (environment or cache), else from PATH, else from
# ARM_GCC_MAC_BIN of tools/versions.env (STM32CubeCLT on the Mac workstation).

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(NOT ARM_GCC_BIN AND DEFINED ENV{ARM_GCC_BIN})
    set(ARM_GCC_BIN "$ENV{ARM_GCC_BIN}")
endif()
if(NOT ARM_GCC_BIN)
    find_program(LS_ARM_GCC_IN_PATH arm-none-eabi-gcc)
    if(LS_ARM_GCC_IN_PATH)
        get_filename_component(ARM_GCC_BIN "${LS_ARM_GCC_IN_PATH}" DIRECTORY)
    else()
        file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../../../tools/versions.env" LS_MAC_BIN_LINE
             REGEX "^ARM_GCC_MAC_BIN=")
        string(REGEX REPLACE "^ARM_GCC_MAC_BIN=" "" ARM_GCC_BIN "${LS_MAC_BIN_LINE}")
    endif()
endif()
set(ARM_GCC_BIN "${ARM_GCC_BIN}" CACHE PATH "Directory of arm-none-eabi-gcc")

set(CMAKE_C_COMPILER "${ARM_GCC_BIN}/arm-none-eabi-gcc")
set(CMAKE_ASM_COMPILER "${ARM_GCC_BIN}/arm-none-eabi-gcc")
set(CMAKE_OBJCOPY "${ARM_GCC_BIN}/arm-none-eabi-objcopy" CACHE FILEPATH "objcopy")
set(CMAKE_SIZE "${ARM_GCC_BIN}/arm-none-eabi-size" CACHE FILEPATH "size")

set(LS_CPU_FLAGS "-mcpu=cortex-m3 -mthumb -mfloat-abi=soft")
set(CMAKE_C_FLAGS_INIT "${LS_CPU_FLAGS} -ffunction-sections -fdata-sections -fno-common")
set(CMAKE_ASM_FLAGS_INIT "${LS_CPU_FLAGS} -x assembler-with-cpp")
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "${LS_CPU_FLAGS} -nostartfiles --specs=nano.specs -Wl,--gc-sections")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
