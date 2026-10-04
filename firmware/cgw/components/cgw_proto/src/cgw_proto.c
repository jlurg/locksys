/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

#include "cgw_proto/cgw_proto.h"

#include <string.h>

#include "pb_decode.h"
#include "pb_encode.h"

cgw_rc_t cgw_proto_decode_frame(const uint8_t *buf, size_t len, cgw_proto_frame_t *out)
{
    cgw_rc_t rc = CGW_E_ARG;

    if ((buf != NULL) && (out != NULL) && (len > 0u) && (len <= CGW_WS_FRAME_MAX))
    {
        locksys_app_v1_Frame msg = locksys_app_v1_Frame_init_zero;
        pb_istream_t stream = pb_istream_from_buffer(buf, len);

        rc = CGW_E_PROTO;
        if (pb_decode(&stream, locksys_app_v1_Frame_fields, &msg) &&
            ((msg.tag.size == 0u) || (msg.tag.size == CGW_TAG_LEN)))
        {
            out->counter = msg.counter;
            (void)memcpy(out->body, msg.body.bytes, msg.body.size);
            out->body_len = (uint16_t)msg.body.size;
            (void)memcpy(out->tag, msg.tag.bytes, msg.tag.size);
            out->tag_len = (uint8_t)msg.tag.size;
            rc = CGW_OK;
        }
    }
    return rc;
}

cgw_rc_t cgw_proto_encode_frame(const cgw_proto_frame_t *frame, uint8_t *out, size_t size,
                                size_t *out_len)
{
    cgw_rc_t rc = CGW_E_ARG;

    if ((frame != NULL) && (out != NULL) && (out_len != NULL) &&
        (frame->body_len <= CGW_PROTO_BODY_MAX) && (frame->tag_len <= CGW_TAG_LEN))
    {
        locksys_app_v1_Frame msg = locksys_app_v1_Frame_init_zero;
        pb_ostream_t stream = pb_ostream_from_buffer(out, size);

        msg.counter = frame->counter;
        msg.body.size = (pb_size_t)frame->body_len;
        (void)memcpy(msg.body.bytes, frame->body, frame->body_len);
        msg.tag.size = (pb_size_t)frame->tag_len;
        (void)memcpy(msg.tag.bytes, frame->tag, frame->tag_len);

        rc = CGW_E_PROTO;
        if (pb_encode(&stream, locksys_app_v1_Frame_fields, &msg))
        {
            *out_len = stream.bytes_written;
            rc = CGW_OK;
        }
    }
    return rc;
}

cgw_rc_t cgw_proto_decode_body(const uint8_t *buf, size_t len, locksys_app_v1_Body *out)
{
    cgw_rc_t rc = CGW_E_ARG;

    if ((out != NULL) && ((buf != NULL) || (len == 0u)))
    {
        pb_istream_t stream = pb_istream_from_buffer(buf, len);

        rc = pb_decode(&stream, locksys_app_v1_Body_fields, out) ? CGW_OK : CGW_E_PROTO;
    }
    return rc;
}

cgw_rc_t cgw_proto_encode_body(const locksys_app_v1_Body *body, uint8_t *out, size_t size,
                               size_t *out_len)
{
    cgw_rc_t rc = CGW_E_ARG;

    if ((body != NULL) && (out != NULL) && (out_len != NULL))
    {
        pb_ostream_t stream = pb_ostream_from_buffer(out, size);

        rc = CGW_E_PROTO;
        if (pb_encode(&stream, locksys_app_v1_Body_fields, body))
        {
            *out_len = stream.bytes_written;
            rc = CGW_OK;
        }
    }
    return rc;
}

bool cgw_proto_is_app_message(pb_size_t which_msg)
{
    bool app_message;

    switch (which_msg)
    {
        case locksys_app_v1_Body_client_auth_tag:
        case locksys_app_v1_Body_ping_tag:
        case locksys_app_v1_Body_pong_tag:
        case locksys_app_v1_Body_door_command_tag:
        case locksys_app_v1_Body_window_move_tag:
        case locksys_app_v1_Body_window_stop_tag:
        case locksys_app_v1_Body_status_request_tag:
            app_message = true;
            break;
        default:
            app_message = false;
            break;
    }
    return app_message;
}

bool cgw_proto_body_is_valid(const locksys_app_v1_Body *body)
{
    bool valid = false;

    if (body != NULL)
    {
        switch (body->which_msg)
        {
            case locksys_app_v1_Body_door_command_tag:
                valid = (uint32_t)body->msg.door_command.action <=
                        (uint32_t)_locksys_app_v1_DoorAction_MAX;
                break;
            case locksys_app_v1_Body_window_move_tag:
                valid = (uint32_t)body->msg.window_move.direction <=
                        (uint32_t)_locksys_app_v1_WindowDirection_MAX;
                break;
            default:
                valid = true;
                break;
        }
    }
    return valid;
}
