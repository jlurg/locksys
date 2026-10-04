/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_can_port.h
 * @brief CAN port: frame transmission and bus-off recovery for cgw_com.
 *
 * Implemented by cgw_can_esp. The can_io task is the only caller; it is the single producer of
 * frames for the TWAI driver.
 */

#ifndef CGW_CAN_PORT_H
#define CGW_CAN_PORT_H

#include <stdint.h>

#include "cgw_ports/cgw_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /** @brief Error state of the CAN controller. */
    typedef enum
    {
        CGW_BUS_ACTIVE = 0, /**< error active */
        CGW_BUS_WARNING,    /**< error warning (TEC or REC >= 96) */
        CGW_BUS_PASSIVE,    /**< error passive */
        CGW_BUS_OFF,        /**< bus-off: no transmission until recovered */
        CGW_BUS_STOPPED     /**< controller not enabled */
    } cgw_bus_state_t;

/** @brief Largest classical CAN payload. */
#define CGW_CAN_DLC_MAX (8u)

    /**
 * @brief Transmit one complete, E2E-protected frame through a static TX slot of its message.
 *
 * @param[in] id   Standard identifier.
 * @param[in] data Frame bytes; copied before the call returns.
 * @param[in] dlc  Data length, 0 .. 8.
 * @retval CGW_OK      Frame handed to the driver.
 * @retval CGW_E_FULL  Both slots of the message are busy; frame not sent.
 * @retval CGW_E_STATE Controller is bus-off or stopped; frame not sent.
 * @retval CGW_E_ARG   Unknown identifier or invalid length.
 */
    cgw_rc_t cgw_can_tx(uint16_t id, const uint8_t *data, uint8_t dlc);

    /**
 * @brief Start bus-off recovery; completion is reported as a bus state event.
 *
 * @retval CGW_OK      Recovery started.
 * @retval CGW_E_STATE Controller is not bus-off.
 */
    cgw_rc_t cgw_can_recover(void);

    /**
 * @brief Rewrite the TX slots of a message that the driver still owns.
 *
 * Used at bus-off entry, before recovery starts: every busy slot of @p id is overwritten in place
 * with the next entry of @p frames, so a frame transmitted after recovery carries the new
 * content (LS-SAIC-001 section 7.5).
 *
 * @param[in] id       Standard identifier.
 * @param[in] frames   Replacement frames, each CGW_CAN_DLC_MAX bytes, used in order.
 * @param[in] n_frames Number of replacement frames.
 * @param[in] dlc      Data length of the replacement frames.
 * @return Number of slots rewritten (at most @p n_frames).
 */
    uint8_t cgw_can_rewrite_busy(uint16_t id, const uint8_t frames[][CGW_CAN_DLC_MAX],
                                 uint8_t n_frames, uint8_t dlc);

    /**
 * @brief Release the TX slots that were busy at bus-off and got no completion.
 *
 * @return Number of slots released.
 */
    uint8_t cgw_can_reclaim_suspect(void);

#ifdef __cplusplus
}
#endif

#endif /* CGW_CAN_PORT_H */
