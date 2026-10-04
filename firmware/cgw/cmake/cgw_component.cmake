# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 jlurg
#
# Compiler warnings of the CGW project components (LS-CGW-SAD-001 section 14).
# Usage, after idf_component_register(): cgw_component_warnings(core) or cgw_component_warnings(adapter).
# Core components (portable, host-tested) add the conversion warnings.

include_guard(GLOBAL)

set(CGW_WARNINGS_COMMON
    -Wall -Wextra -Werror -Wshadow -Wundef -Wcast-align -Wstrict-prototypes
    -Wmissing-prototypes -Wformat=2 -Wvla -Wnull-dereference
)
set(CGW_WARNINGS_CORE -Wconversion -Wsign-conversion)

function(cgw_component_warnings kind)
    target_compile_options(${COMPONENT_LIB} PRIVATE ${CGW_WARNINGS_COMMON})
    if(kind STREQUAL "core")
        target_compile_options(${COMPONENT_LIB} PRIVATE ${CGW_WARNINGS_CORE})
    elseif(NOT kind STREQUAL "adapter")
        message(FATAL_ERROR "cgw_component_warnings: unknown kind '${kind}'")
    endif()
endfunction()
