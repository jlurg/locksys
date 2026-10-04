/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_canio_port.h
 * @brief can_io port: requests from the core task to the can_io task.
 *
 * Implemented by the composition root (posts to the can_io queue). The core task is the only
 * caller.
 */

#ifndef CGW_CANIO_PORT_H
#define CGW_CANIO_PORT_H

#include <stdint.h>

#include "cgw_ports/cgw_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
 * @brief Pass the current window intent to can_io.
 *
 * Posted at the front of the can_io queue on every change and every keep-alive refresh.
 *
 * @param[in] intent Intent; copied.
 * @retval CGW_OK     Posted.
 * @retval CGW_E_FULL Queue full; the caller retries on its next tick.
 */
    cgw_rc_t cgw_canio_post_intent(const cgw_win_intent_t *intent);

    /**
 * @brief Request one CGW_DoorCmd transaction (three transmissions).
 *
 * @param[in] req_id  CAN ReqId, 1 .. 255.
 * @param[in] request Ls_DoorRequestType: LOCK or UNLOCK.
 * @retval CGW_OK     Posted.
 * @retval CGW_E_FULL Queue full.
 */
    cgw_rc_t cgw_canio_post_door_request(uint8_t req_id, uint8_t request);

    /**
 * @brief Update the content of CGW_NodeSts.
 *
 * @param[in] info Node information; copied.
 * @retval CGW_OK     Posted.
 * @retval CGW_E_FULL Queue full.
 */
    cgw_rc_t cgw_canio_post_node_info(const cgw_node_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* CGW_CANIO_PORT_H */
