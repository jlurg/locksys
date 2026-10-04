/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_fi.h
 * @brief Fault injection for HIL evidence (DEV builds only, CONFIG_CGW_FAULT_INJECTION).
 *
 * RC and RELEASE images contain no cgw_fi_ symbol (SWR-CGW-067); every user is guarded by
 * CONFIG_CGW_FAULT_INJECTION.
 */

#ifndef CGW_FI_H
#define CGW_FI_H

#include <stdbool.h>
#include <stdint.h>

#include "cgw_ports/cgw_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Active fault injections, set by the console and read by the tasks. */
    typedef struct
    {
        volatile uint32_t
            core_hang_ms;         /**< fi core_hang <ms>: next core dispatch blocks this long */
        volatile bool canio_hang; /**< fi canio_hang: can_io stops (task watchdog test) */
        volatile bool can_silent; /**< fi can_silent: can_io transmits nothing */
        volatile uint8_t e2e_crc; /**< fi e2e_crc <n>: corrupt the CRC of the next n frames */
        volatile uint8_t e2e_ctr; /**< fi e2e_ctr <n>: repeat the alive counter n times */
        volatile bool drop_stop;  /**< fi drop_stop: suppress STOP frames */
        volatile bool wifi_off;   /**< fi wifi_off: stop the SoftAP */
    } cgw_fi_state_t;

    /** @brief Fault-injection state (DEV builds only). */
    extern cgw_fi_state_t cgw_fi_state;

    /**
 * @brief Register the "fi" console command and start the console REPL on UART0.
 *
 * @retval CGW_OK   Console running.
 * @retval CGW_E_IO Console could not be started.
 */
    cgw_rc_t cgw_fi_init(void);

#ifdef __cplusplus
}
#endif

#endif /* CGW_FI_H */
