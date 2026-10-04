/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/* cgw_health host tests: DTC status bits, confirmation, modes and reset mapping. */

#include "cgw_health/cgw_health.h"
#include "ls_dtc_gen.h"
#include "ls_enums_gen.h"
#include "ls_params_gen.h"
#include "unity.h"

static cgw_health_t h;
static cgw_health_inputs_t in;

void setUp(void)
{
    cgw_health_init(&h);
    in.ap_started = true;
    in.can_active = true;
    in.dcu_alive = true;
    in.dcu_seen = true;
    in.version_fault = false;
}

void tearDown(void)
{
}

/* @verifies SWR-CGW-061 */
void test_dtc_status_bits_and_notice_changes(void)
{
    TEST_ASSERT_TRUE(cgw_health_report(&h, CGW_DTC_BUS_OFF, true, 10u));
    TEST_ASSERT_FALSE(cgw_health_report(&h, CGW_DTC_BUS_OFF, true, 20u));
    TEST_ASSERT_EQUAL_HEX8(CGW_DTC_ST_TEST_FAILED | CGW_DTC_ST_PENDING | CGW_DTC_ST_CONFIRMED |
                               CGW_DTC_ST_FAILED_SINCE,
                           h.status[CGW_DTC_BUS_OFF]);
    TEST_ASSERT_TRUE(cgw_health_report(&h, CGW_DTC_BUS_OFF, false, 30u));
    TEST_ASSERT_EQUAL_HEX8(CGW_DTC_ST_PENDING | CGW_DTC_ST_CONFIRMED | CGW_DTC_ST_FAILED_SINCE,
                           h.status[CGW_DTC_BUS_OFF]);
    TEST_ASSERT_EQUAL_UINT8(1u, cgw_health_dtc_count(&h));
    TEST_ASSERT_FALSE(cgw_health_report(&h, CGW_DTC_COUNT, true, 0u));
    TEST_ASSERT_EQUAL_HEX32(LS_DTC_U1B01_88, cgw_health_dtc_value(CGW_DTC_BUS_OFF));
    TEST_ASSERT_EQUAL_HEX32(0u, cgw_health_dtc_value(CGW_DTC_COUNT));
    TEST_ASSERT_EQUAL_UINT8(LS_FAULT_SEVERITY_CRITICAL, cgw_health_dtc_severity(CGW_DTC_TWAI));
    TEST_ASSERT_EQUAL_UINT8(LS_FAULT_SEVERITY_INFO, cgw_health_dtc_severity(CGW_DTC_COUNT));
}

/* @verifies SWR-CGW-049 */
void test_dcu_lost_confirms_after_delay(void)
{
    (void)cgw_health_report(&h, CGW_DTC_DCU_LOST, true, 100u);
    cgw_health_tick(&h, 100u + LS_T_COMM_DTC_CONFIRM_MS - 1u);
    TEST_ASSERT_EQUAL_UINT8(0u, cgw_health_dtc_count(&h));
    cgw_health_tick(&h, 100u + LS_T_COMM_DTC_CONFIRM_MS);
    TEST_ASSERT_EQUAL_UINT8(1u, cgw_health_dtc_count(&h));
}

/* @verifies SWR-CGW-060 */
void test_modes(void)
{
    in.dcu_seen = false;
    TEST_ASSERT_EQUAL_UINT8(LS_NODE_MODE_INIT, cgw_health_update_mode(&h, &in));
    in.dcu_seen = true;
    TEST_ASSERT_EQUAL_UINT8(LS_NODE_MODE_NORMAL, cgw_health_update_mode(&h, &in));
    in.dcu_alive = false;
    TEST_ASSERT_EQUAL_UINT8(LS_NODE_MODE_DEGRADED, cgw_health_update_mode(&h, &in));
    in.dcu_alive = true;
    TEST_ASSERT_EQUAL_UINT8(LS_NODE_MODE_NORMAL, cgw_health_update_mode(&h, &in));
    (void)cgw_health_report(&h, CGW_DTC_NVS, true, 0u);
    TEST_ASSERT_EQUAL_UINT8(LS_NODE_MODE_DEGRADED, cgw_health_update_mode(&h, &in));
    (void)cgw_health_report(&h, CGW_DTC_CRYPTO, true, 0u);
    TEST_ASSERT_EQUAL_UINT8(LS_NODE_MODE_SAFE, cgw_health_update_mode(&h, &in));
    TEST_ASSERT_EQUAL_UINT8(LS_NODE_MODE_SAFE, h.mode);
}

void test_version_fault_before_normal_is_degraded(void)
{
    in.version_fault = true;
    TEST_ASSERT_EQUAL_UINT8(LS_NODE_MODE_DEGRADED, cgw_health_update_mode(&h, &in));
}

/* @verifies SWR-CGW-062 */
void test_reset_mapping(void)
{
    cgw_dtc_t dtc;

    TEST_ASSERT_EQUAL_UINT8(LS_RESET_REASON_POWER_ON, cgw_health_map_reset(CGW_RST_POWERON, &dtc));
    TEST_ASSERT_EQUAL(CGW_DTC_COUNT, dtc);
    TEST_ASSERT_EQUAL_UINT8(LS_RESET_REASON_PIN, cgw_health_map_reset(CGW_RST_USB, &dtc));
    TEST_ASSERT_EQUAL_UINT8(LS_RESET_REASON_SOFTWARE, cgw_health_map_reset(CGW_RST_SW, &dtc));
    TEST_ASSERT_EQUAL_UINT8(LS_RESET_REASON_SOFTWARE, cgw_health_map_reset(CGW_RST_PANIC, &dtc));
    TEST_ASSERT_EQUAL(CGW_DTC_PANIC, dtc);
    TEST_ASSERT_EQUAL_UINT8(LS_RESET_REASON_WATCHDOG, cgw_health_map_reset(CGW_RST_TASK_WDT, &dtc));
    TEST_ASSERT_EQUAL(CGW_DTC_WATCHDOG, dtc);
    TEST_ASSERT_EQUAL_UINT8(LS_RESET_REASON_LOW_POWER,
                            cgw_health_map_reset(CGW_RST_DEEPSLEEP, &dtc));
    TEST_ASSERT_EQUAL_UINT8(LS_RESET_REASON_BROWNOUT,
                            cgw_health_map_reset(CGW_RST_PWR_GLITCH, &dtc));
    TEST_ASSERT_EQUAL(CGW_DTC_BROWNOUT, dtc);
    TEST_ASSERT_EQUAL_UINT8(LS_RESET_REASON_UNKNOWN, cgw_health_map_reset(CGW_RST_UNKNOWN, &dtc));
}

void test_reset_storm_latches_safe(void)
{
    cgw_health_check_reset_storm(&h, LS_N_WDT_RESET_SAFE - 1u);
    TEST_ASSERT_FALSE(h.safe);
    cgw_health_check_reset_storm(&h, LS_N_WDT_RESET_SAFE);
    TEST_ASSERT_EQUAL_UINT8(LS_NODE_MODE_SAFE, cgw_health_update_mode(&h, &in));
}
