/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "cgw_health/cgw_health.h"

#include <string.h>

#include "ls_dtc_gen.h"
#include "ls_enums_gen.h"
#include "ls_params_gen.h"

typedef struct
{
    uint32_t value;
    uint32_t confirm_ms;
    uint8_t severity;
} health_dtc_def_t;

static const health_dtc_def_t k_dtc[CGW_DTC_COUNT] = {
    {LS_DTC_U1B00_87, LS_T_COMM_DTC_CONFIRM_MS, LS_DTC_U1B00_87_SEVERITY},
    {LS_DTC_U1B01_88, 0u, LS_DTC_U1B01_88_SEVERITY},
    {LS_DTC_U1B02_82, 0u, LS_DTC_U1B02_82_SEVERITY},
    {LS_DTC_U1B02_83, 0u, LS_DTC_U1B02_83_SEVERITY},
    {LS_DTC_U1B03_56, 0u, LS_DTC_U1B03_56_SEVERITY},
    {LS_DTC_B1B10_96, 0u, LS_DTC_B1B10_96_SEVERITY},
    {LS_DTC_B1B11_96, 0u, LS_DTC_B1B11_96_SEVERITY},
    {LS_DTC_B1B12_47, 0u, LS_DTC_B1B12_47_SEVERITY},
    {LS_DTC_B1B13_16, 0u, LS_DTC_B1B13_16_SEVERITY},
    {LS_DTC_B1B14_47, 0u, LS_DTC_B1B14_47_SEVERITY},
    {LS_DTC_B1B15_44, 0u, LS_DTC_B1B15_44_SEVERITY},
    {LS_DTC_B1B16_96, 0u, LS_DTC_B1B16_96_SEVERITY},
};

void cgw_health_init(cgw_health_t *h)
{
    (void)memset(h, 0, sizeof(*h));
    h->mode = LS_NODE_MODE_INIT;
}

/* @satisfies SWR-CGW-061 */
bool cgw_health_report(cgw_health_t *h, cgw_dtc_t dtc, bool failed, uint32_t now_ms)
{
    bool changed = false;

    if (dtc < CGW_DTC_COUNT)
    {
        uint8_t *st = &h->status[dtc];
        bool was_failed = (*st & CGW_DTC_ST_TEST_FAILED) != 0u;

        changed = (was_failed != failed);
        if (failed && !was_failed)
        {
            *st = (uint8_t)(*st | CGW_DTC_ST_TEST_FAILED | CGW_DTC_ST_PENDING |
                            CGW_DTC_ST_FAILED_SINCE);
            h->t_fail_ms[dtc] = now_ms;
            if (k_dtc[dtc].confirm_ms == 0u)
            {
                *st = (uint8_t)(*st | CGW_DTC_ST_CONFIRMED);
            }
            if (k_dtc[dtc].severity == LS_FAULT_SEVERITY_CRITICAL)
            {
                h->safe = true;
            }
        }
        else if (!failed)
        {
            *st = (uint8_t)(*st & (uint8_t)~CGW_DTC_ST_TEST_FAILED);
        }
        else
        {
            /* Still failed. */
        }
    }
    return changed;
}

void cgw_health_tick(cgw_health_t *h, uint32_t now_ms)
{
    for (uint32_t i = 0u; i < (uint32_t)CGW_DTC_COUNT; i++)
    {
        if (((h->status[i] & CGW_DTC_ST_TEST_FAILED) != 0u) &&
            ((uint32_t)(now_ms - h->t_fail_ms[i]) >= k_dtc[i].confirm_ms))
        {
            h->status[i] = (uint8_t)(h->status[i] | CGW_DTC_ST_CONFIRMED);
        }
    }
}

uint8_t cgw_health_dtc_count(const cgw_health_t *h)
{
    uint8_t n = 0u;

    for (uint32_t i = 0u; i < (uint32_t)CGW_DTC_COUNT; i++)
    {
        if ((h->status[i] & CGW_DTC_ST_CONFIRMED) != 0u)
        {
            n++;
        }
    }
    return n;
}

uint32_t cgw_health_dtc_value(cgw_dtc_t dtc)
{
    return (dtc < CGW_DTC_COUNT) ? k_dtc[dtc].value : 0u;
}

uint8_t cgw_health_dtc_severity(cgw_dtc_t dtc)
{
    return (dtc < CGW_DTC_COUNT) ? k_dtc[dtc].severity : LS_FAULT_SEVERITY_INFO;
}

static bool health_failed(const cgw_health_t *h, cgw_dtc_t dtc)
{
    return (h->status[dtc] & CGW_DTC_ST_TEST_FAILED) != 0u;
}

/* @satisfies SWR-CGW-060 */
uint8_t cgw_health_update_mode(cgw_health_t *h, const cgw_health_inputs_t *in)
{
    bool ready = in->ap_started && in->can_active && in->dcu_seen && !in->version_fault;
    bool degraded_dtc = health_failed(h, CGW_DTC_SOFTAP) || health_failed(h, CGW_DTC_NVS);

    if (h->safe)
    {
        h->mode = LS_NODE_MODE_SAFE;
    }
    else if (ready && in->dcu_alive && !degraded_dtc)
    {
        h->mode = LS_NODE_MODE_NORMAL;
        h->ever_normal = true;
    }
    else if (h->ever_normal || degraded_dtc || in->version_fault)
    {
        h->mode = LS_NODE_MODE_DEGRADED;
    }
    else
    {
        h->mode = LS_NODE_MODE_INIT;
    }
    return h->mode;
}

/* @satisfies SWR-CGW-062 */
uint8_t cgw_health_map_reset(cgw_reset_src_t src, cgw_dtc_t *dtc)
{
    uint8_t reason;

    *dtc = CGW_DTC_COUNT;
    switch (src)
    {
        case CGW_RST_POWERON:
            reason = LS_RESET_REASON_POWER_ON;
            break;
        case CGW_RST_EXT:
        case CGW_RST_USB:
        case CGW_RST_JTAG:
            reason = LS_RESET_REASON_PIN;
            break;
        case CGW_RST_SW:
            reason = LS_RESET_REASON_SOFTWARE;
            break;
        case CGW_RST_PANIC:
        case CGW_RST_CPU_LOCKUP:
            reason = LS_RESET_REASON_SOFTWARE;
            *dtc = CGW_DTC_PANIC;
            break;
        case CGW_RST_INT_WDT:
        case CGW_RST_TASK_WDT:
        case CGW_RST_WDT:
            reason = LS_RESET_REASON_WATCHDOG;
            *dtc = CGW_DTC_WATCHDOG;
            break;
        case CGW_RST_DEEPSLEEP:
            reason = LS_RESET_REASON_LOW_POWER;
            break;
        case CGW_RST_BROWNOUT:
        case CGW_RST_PWR_GLITCH:
            reason = LS_RESET_REASON_BROWNOUT;
            *dtc = CGW_DTC_BROWNOUT;
            break;
        default:
            reason = LS_RESET_REASON_UNKNOWN;
            break;
    }
    return reason;
}

void cgw_health_check_reset_storm(cgw_health_t *h, uint8_t resets_in_window)
{
    if (resets_in_window >= LS_N_WDT_RESET_SAFE)
    {
        h->safe = true;
    }
}
