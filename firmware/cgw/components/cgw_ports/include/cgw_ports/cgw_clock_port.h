/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_clock_port.h
 * @brief Clock port: monotonic millisecond time base.
 *
 * Implemented by cgw_platform_esp on the target and by a fake in the host tests.
 */

#ifndef CGW_CLOCK_PORT_H
#define CGW_CLOCK_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
 * @brief Monotonic time since boot.
 *
 * @return Milliseconds, wrapping modulo 2^32. Times are compared only through unsigned
 *         differences (cgw_time_reached()).
 * @note Callable from any task; not from interrupt handlers.
 */
    uint32_t cgw_clock_now_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* CGW_CLOCK_PORT_H */
