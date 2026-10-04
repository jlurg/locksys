/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_proto.h
 * @brief APP protocol 1.0 codec: Frame and Body encoding and decoding with nanopb.
 *
 * Receive order (LS-SAIC-001 section 8.2): raw payload -> Frame (cgw_proto_decode_frame()) ->
 * tag over the raw body (cgw_session) -> counter -> Body (cgw_proto_decode_body()) -> enum and
 * range validation (cgw_proto_body_is_valid()). Unknown fields are ignored; an unknown Body
 * member decodes with which_msg = 0.
 */

#ifndef CGW_PROTO_H
#define CGW_PROTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cgw_ports/cgw_types.h"
#include "locksys_app.pb.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @name Protocol version (package locksys.app.v1) @{ */
#define CGW_PROTO_MAJOR (1u)
#define CGW_PROTO_MINOR (0u)
/** @} */

/** @brief Largest serialised Body (Frame.body max_size in locksys_app.options). */
#define CGW_PROTO_BODY_MAX (224u)

    /** @brief Frame with its raw body and tag. */
    typedef struct
    {
        uint32_t counter;                 /**< frame counter; 0 for handshake frames */
        uint8_t body[CGW_PROTO_BODY_MAX]; /**< raw serialised Body */
        uint8_t tag[CGW_TAG_LEN];         /**< frame tag; empty for handshake frames */
        uint16_t body_len;                /**< body length */
        uint8_t tag_len;                  /**< tag length, 0 or CGW_TAG_LEN */
    } cgw_proto_frame_t;

    /**
 * @brief Decode one WebSocket message into a Frame.
 *
 * @param[in]  buf Message bytes.
 * @param[in]  len Message length, 1 .. CGW_WS_FRAME_MAX.
 * @param[out] out Frame.
 * @retval CGW_OK      Frame decoded; the tag length is 0 or CGW_TAG_LEN.
 * @retval CGW_E_PROTO Malformed Frame, oversized field or invalid tag length.
 * @retval CGW_E_ARG   Invalid argument.
 */
    cgw_rc_t cgw_proto_decode_frame(const uint8_t *buf, size_t len, cgw_proto_frame_t *out);

    /**
 * @brief Encode a Frame into a WebSocket message.
 *
 * @param[in]  frame   Frame; counter 0 and an empty tag are omitted (proto3 defaults).
 * @param[out] out     Destination.
 * @param[in]  size    Size of @p out.
 * @param[out] out_len Encoded length.
 * @retval CGW_OK      Encoded.
 * @retval CGW_E_PROTO Encoding failed (buffer too small).
 * @retval CGW_E_ARG   Invalid argument.
 */
    cgw_rc_t cgw_proto_encode_frame(const cgw_proto_frame_t *frame, uint8_t *out, size_t size,
                                    size_t *out_len);

    /**
 * @brief Decode a raw Body.
 *
 * @param[in]  buf Raw body.
 * @param[in]  len Body length.
 * @param[out] out Body; which_msg is 0 for an unknown member.
 * @retval CGW_OK      Decoded.
 * @retval CGW_E_PROTO Malformed Body.
 * @retval CGW_E_ARG   Invalid argument.
 */
    cgw_rc_t cgw_proto_decode_body(const uint8_t *buf, size_t len, locksys_app_v1_Body *out);

    /**
 * @brief Encode a Body.
 *
 * @param[in]  body    Body.
 * @param[out] out     Destination.
 * @param[in]  size    Size of @p out.
 * @param[out] out_len Encoded length.
 * @retval CGW_OK      Encoded.
 * @retval CGW_E_PROTO Encoding failed.
 * @retval CGW_E_ARG   Invalid argument.
 */
    cgw_rc_t cgw_proto_encode_body(const locksys_app_v1_Body *body, uint8_t *out, size_t size,
                                   size_t *out_len);

    /**
 * @brief Test whether a Body member may be sent by the APP.
 *
 * @param[in] which_msg Body member tag.
 * @return true for ClientAuth, Ping, Pong, DoorCommand, WindowMove, WindowStop and
 *         StatusRequest.
 */
    bool cgw_proto_is_app_message(pb_size_t which_msg);

    /**
 * @brief Validate the enumerations of an APP -> CGW Body (proto3 enums are open).
 *
 * @param[in] body Decoded body.
 * @return true when every enumeration value is defined; false otherwise or for NULL.
 */
    bool cgw_proto_body_is_valid(const locksys_app_v1_Body *body);

#ifdef __cplusplus
}
#endif

#endif /* CGW_PROTO_H */
