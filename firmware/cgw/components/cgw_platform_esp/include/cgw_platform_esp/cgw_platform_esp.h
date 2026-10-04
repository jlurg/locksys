/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_platform_esp.h
 * @brief ESP-IDF platform adapter: active-object runtime (static tasks and queues), clock, trace
 *        outputs, reset source, alive supervision, heap statistics and device identity.
 *
 * Implements cgw_clock_port.h, cgw_trace_port.h and cgw_system_port.h.
 */

#ifndef CGW_PLATFORM_ESP_H
#define CGW_PLATFORM_ESP_H

#include <stdbool.h>
#include <stdint.h>

#include "cgw_health/cgw_health.h"
#include "cgw_ports/cgw_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Static definition of one active object (task plus input queue). */
    typedef struct
    {
        const char *name;      /**< task name */
        TaskFunction_t fn;     /**< task body */
        void *arg;             /**< task argument */
        StackType_t *stack;    /**< stack storage */
        StaticTask_t *tcb;     /**< task control block storage */
        uint8_t *q_storage;    /**< queue storage: q_len * item_size bytes */
        StaticQueue_t *q_buf;  /**< queue control block storage */
        uint32_t stack_bytes;  /**< stack size */
        UBaseType_t prio;      /**< priority */
        BaseType_t core;       /**< CPU */
        UBaseType_t q_len;     /**< queue length */
        UBaseType_t item_size; /**< queue item size */
    } cgw_ao_def_t;

    /** @brief Running active object. */
    typedef struct
    {
        QueueHandle_t q;   /**< input queue */
        TaskHandle_t task; /**< task */
    } cgw_ao_t;

    /** @brief Tasks under alive supervision (SM-16). */
    typedef enum
    {
        CGW_ALIVE_CORE = 0, /**< core task */
        CGW_ALIVE_CANIO,    /**< can_io task */
        CGW_ALIVE_COUNT     /**< number of supervised tasks */
    } cgw_alive_task_t;

    /** @brief Trace output pins; negative values disable an output. */
    typedef struct
    {
        int trace0; /**< RMT width-coded events */
        int trace1; /**< STOP pending */
        int trace2; /**< CGW_WinCmd enqueue pulse */
        int trace3; /**< core dispatch */
    } cgw_trace_cfg_t;

    /**
 * @brief Create the queue and the task of an active object, both static.
 *
 * The queue is created before the task starts.
 *
 * @param[in]  def Definition.
 * @param[out] ao  Handles.
 * @retval CGW_OK    Started.
 * @retval CGW_E_ARG Invalid definition.
 */
    cgw_rc_t cgw_platform_ao_start(const cgw_ao_def_t *def, cgw_ao_t *ao);

    /**
 * @brief Post an event to the back of an active-object queue.
 *
 * @param[in] ao      Active object.
 * @param[in] ev      Event; copied.
 * @param[in] wait_ms Bounded wait (0 for data events).
 * @retval CGW_OK     Posted.
 * @retval CGW_E_FULL Queue full after the wait.
 */
    cgw_rc_t cgw_platform_ao_post(const cgw_ao_t *ao, const void *ev, uint32_t wait_ms);

    /**
 * @brief Post an event to the front of an active-object queue.
 *
 * @param[in] ao      Active object.
 * @param[in] ev      Event; copied.
 * @param[in] wait_ms Bounded wait.
 * @retval CGW_OK     Posted.
 * @retval CGW_E_FULL Queue full after the wait.
 */
    cgw_rc_t cgw_platform_ao_post_front(const cgw_ao_t *ao, const void *ev, uint32_t wait_ms);

    /**
 * @brief Configure the trace outputs (all builds, LS-SAIC-001 section 3.5).
 *
 * @param[in] cfg Pins.
 * @retval CGW_OK   Configured.
 * @retval CGW_E_IO GPIO or RMT error.
 */
    cgw_rc_t cgw_platform_trace_init(const cgw_trace_cfg_t *cfg);

    /**
 * @brief Reset source of this start.
 *
 * @return Reset source.
 */
    cgw_reset_src_t cgw_platform_reset_source(void);

    /**
 * @brief Count watchdog and panic resets within t_wdt_reset_window_ms (RTC_NOINIT record, per
 *        power cycle).
 *
 * @param[in] src Reset source of this start.
 * @return Resets within the window, including this one.
 */
    uint8_t cgw_platform_reset_storm_count(cgw_reset_src_t src);

    /**
 * @brief Signal that a supervised task completed a cycle.
 *
 * @param[in] task Task.
 */
    void cgw_platform_alive_kick(cgw_alive_task_t task);

    /**
 * @brief Check the alive counters of the supervised tasks.
 *
 * @param[in] now_ms Current time.
 * @return false when a task did not kick within t_cgw_alive_deadline_ms.
 */
    bool cgw_platform_alive_ok(uint32_t now_ms);

    /**
 * @brief Free internal heap in percent of the total internal heap.
 *
 * @return 0 .. 100.
 */
    uint8_t cgw_platform_heap_free_pct(void);

    /**
 * @brief Device identifier: first 8 bytes of SHA-256 over the eFuse base MAC.
 *
 * @param[out] out CGW_DEVICE_ID_LEN bytes.
 * @retval CGW_OK   Computed.
 * @retval CGW_E_IO MAC or hash error.
 */
    cgw_rc_t cgw_platform_device_id(uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif /* CGW_PLATFORM_ESP_H */
