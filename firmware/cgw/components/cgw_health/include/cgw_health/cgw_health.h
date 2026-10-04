/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_health.h
 * @brief CGW mode (LS-SAIC-001 section 6.2), DTC table with status bits 0, 2, 3 and 5
 *        (section 13), and reset-reason mapping.
 *
 * DTCs are kept in RAM (no NvM in the MVP). Every testFailed change is reported to the caller,
 * which sends a Notice with the 24-bit DTC value.
 */

#ifndef CGW_HEALTH_H
#define CGW_HEALTH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief CGW DTCs (LS-SAIC-001 section 13.3). */
    typedef enum
    {
        CGW_DTC_DCU_LOST = 0, /**< U1B00-87 lost communication with the DCU */
        CGW_DTC_BUS_OFF,      /**< U1B01-88 CAN bus-off */
        CGW_DTC_E2E_SEQUENCE, /**< U1B02-82 E2E alive-counter errors */
        CGW_DTC_E2E_CRC,      /**< U1B02-83 E2E CRC or DataID errors */
        CGW_DTC_VERSION,      /**< U1B03-56 CAN matrix version mismatch */
        CGW_DTC_SOFTAP,       /**< B1B10-96 SoftAP start failure */
        CGW_DTC_TWAI,         /**< B1B11-96 TWAI initialisation or driver fault */
        CGW_DTC_WATCHDOG,     /**< B1B12-47 watchdog reset */
        CGW_DTC_BROWNOUT,     /**< B1B13-16 brown-out reset */
        CGW_DTC_PANIC,        /**< B1B14-47 panic reset */
        CGW_DTC_NVS,          /**< B1B15-44 NVS or key store fault */
        CGW_DTC_CRYPTO,       /**< B1B16-96 cryptographic self-test failure */
        CGW_DTC_COUNT         /**< number of CGW DTCs */
    } cgw_dtc_t;

/** @name DTC status bits (ISO 14229-1 subset) @{ */
#define CGW_DTC_ST_TEST_FAILED  (0x01u) /**< bit 0 testFailed */
#define CGW_DTC_ST_PENDING      (0x04u) /**< bit 2 pendingDTC */
#define CGW_DTC_ST_CONFIRMED    (0x08u) /**< bit 3 confirmedDTC */
#define CGW_DTC_ST_FAILED_SINCE (0x20u) /**< bit 5 testFailedSinceLastClear */
    /** @} */

    /** @brief Reset source as reported by the platform (one value per esp_reset_reason_t). */
    typedef enum
    {
        CGW_RST_UNKNOWN = 0, /**< ESP_RST_UNKNOWN, SDIO, EFUSE */
        CGW_RST_POWERON,     /**< power-on, including EN pin resets on ESP32-S3 */
        CGW_RST_EXT,         /**< external pin */
        CGW_RST_USB,         /**< USB-UART or USB-Serial-JTAG reset */
        CGW_RST_JTAG,        /**< JTAG */
        CGW_RST_SW,          /**< esp_restart() */
        CGW_RST_PANIC,       /**< exception or panic */
        CGW_RST_CPU_LOCKUP,  /**< CPU lock-up */
        CGW_RST_INT_WDT,     /**< interrupt watchdog */
        CGW_RST_TASK_WDT,    /**< task watchdog */
        CGW_RST_WDT,         /**< other watchdogs */
        CGW_RST_DEEPSLEEP,   /**< wake from deep sleep */
        CGW_RST_BROWNOUT,    /**< brown-out */
        CGW_RST_PWR_GLITCH   /**< power glitch */
    } cgw_reset_src_t;

    /** @brief Inputs of the mode decision. */
    typedef struct
    {
        bool ap_started;    /**< WIFI_EVENT_AP_START received */
        bool can_active;    /**< TWAI node enabled and not bus-off */
        bool dcu_alive;     /**< DCU_NodeSts fresh */
        bool dcu_seen;      /**< at least one DCU_NodeSts received */
        bool version_fault; /**< CAN matrix major version mismatch */
    } cgw_health_inputs_t;

    /** @brief Health state. Initialise with cgw_health_init(). */
    typedef struct
    {
        uint32_t t_fail_ms[CGW_DTC_COUNT]; /**< time testFailed was set */
        uint8_t status[CGW_DTC_COUNT];     /**< DTC status bytes */
        uint8_t mode;                      /**< Ls_NodeModeType */
        bool ever_normal;                  /**< NORMAL reached since reset */
        bool safe;                         /**< SAFE latch; cleared only by a reset */
    } cgw_health_t;

    /**
 * @brief Initialise: mode INIT, all DTCs passed.
 *
 * @param[out] h Health state.
 */
    void cgw_health_init(cgw_health_t *h);

    /**
 * @brief Report a test result.
 *
 * A failed critical test (TWAI, crypto) latches SAFE. Confirmation follows at once, except for
 * U1B00 which confirms after t_comm_dtc_confirm_ms (cgw_health_tick()).
 *
 * @param[in,out] h      Health state.
 * @param[in]     dtc    DTC.
 * @param[in]     failed Test failed.
 * @param[in]     now_ms Current time.
 * @return true when the testFailed bit changed (send a Notice).
 */
    bool cgw_health_report(cgw_health_t *h, cgw_dtc_t dtc, bool failed, uint32_t now_ms);

    /**
 * @brief Confirm delayed DTCs.
 *
 * @param[in,out] h      Health state.
 * @param[in]     now_ms Current time.
 */
    void cgw_health_tick(cgw_health_t *h, uint32_t now_ms);

    /**
 * @brief Number of confirmed DTCs.
 *
 * @param[in] h Health state.
 * @return Count, at most CGW_DTC_COUNT.
 */
    uint8_t cgw_health_dtc_count(const cgw_health_t *h);

    /**
 * @brief 24-bit value of a DTC (J2012 code and failure type byte).
 *
 * @param[in] dtc DTC.
 * @return Value, 0 for an invalid DTC.
 */
    uint32_t cgw_health_dtc_value(cgw_dtc_t dtc);

    /**
 * @brief Severity of a DTC.
 *
 * @param[in] dtc DTC.
 * @return Ls_FaultSeverityType.
 */
    uint8_t cgw_health_dtc_severity(cgw_dtc_t dtc);

    /**
 * @brief Evaluate the CGW mode.
 *
 * SAFE while latched. INIT until AP started, CAN active and a first DCU_NodeSts with a matching
 * major version. NORMAL while all of them hold and the DCU is alive; DEGRADED otherwise or while a
 * DEGRADED DTC (SoftAP, NVS) is failed.
 *
 * @param[in,out] h  Health state.
 * @param[in]     in Inputs.
 * @return Ls_NodeModeType.
 */
    uint8_t cgw_health_update_mode(cgw_health_t *h, const cgw_health_inputs_t *in);

    /**
 * @brief Map a reset source to ResetReason and its DTC (LS-SAIC-001 section 6.2).
 *
 * @param[in]  src Reset source.
 * @param[out] dtc DTC to report, or CGW_DTC_COUNT for none.
 * @return Ls_ResetReasonType.
 */
    uint8_t cgw_health_map_reset(cgw_reset_src_t src, cgw_dtc_t *dtc);

    /**
 * @brief Latch SAFE after a watchdog or panic reset storm.
 *
 * @param[in,out] h              Health state.
 * @param[in]     resets_in_window Watchdog and panic resets within t_wdt_reset_window_ms.
 */
    void cgw_health_check_reset_storm(cgw_health_t *h, uint8_t resets_in_window);

#ifdef __cplusplus
}
#endif

#endif /* CGW_HEALTH_H */
