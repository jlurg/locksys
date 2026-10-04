/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/* cgw_proto host tests: Frame and Body codec, direction and enumeration validation. */

#include <string.h>

#include "cgw_proto/cgw_proto.h"
#include "fake_ports.h"
#include "unity.h"

TEST_SOURCE_FILE("pb_common.c")
TEST_SOURCE_FILE("pb_decode.c")
TEST_SOURCE_FILE("pb_encode.c")
TEST_SOURCE_FILE("locksys_app.pb.c")

/* window_move_first of interfaces/vectors/app_session_v1.json. */
static const char *const V_MOVE_FIRST_FRAME =
    "080112065a04080110011a109a8bb35b86289aaa442691d8d8ef5b19";

void setUp(void)
{
}

void tearDown(void)
{
}

/* @verifies SWR-CGW-072 */
void test_frame_round_trip_matches_vector(void)
{
    uint8_t buf[CGW_WS_FRAME_MAX];
    uint8_t out[CGW_WS_FRAME_MAX];
    size_t len = fake_hex(V_MOVE_FIRST_FRAME, buf, sizeof(buf));
    size_t out_len = 0u;
    cgw_proto_frame_t fr;
    locksys_app_v1_Body body = locksys_app_v1_Body_init_zero;

    TEST_ASSERT_EQUAL(CGW_OK, cgw_proto_decode_frame(buf, len, &fr));
    TEST_ASSERT_EQUAL_UINT32(1u, fr.counter);
    TEST_ASSERT_EQUAL_UINT8(CGW_TAG_LEN, fr.tag_len);
    TEST_ASSERT_EQUAL(CGW_OK, cgw_proto_decode_body(fr.body, fr.body_len, &body));
    TEST_ASSERT_EQUAL(locksys_app_v1_Body_window_move_tag, body.which_msg);
    TEST_ASSERT_EQUAL_UINT32(1u, body.msg.window_move.press_id);
    TEST_ASSERT_TRUE(cgw_proto_is_app_message(body.which_msg));
    TEST_ASSERT_TRUE(cgw_proto_body_is_valid(&body));
    TEST_ASSERT_EQUAL(CGW_OK, cgw_proto_encode_frame(&fr, out, sizeof(out), &out_len));
    TEST_ASSERT_EQUAL_size_t(len, out_len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(buf, out, len);
}

/* @verifies SWR-CGW-072 */
void test_malformed_and_invalid_input_rejected(void)
{
    static const uint8_t garbage[] = {0xFFu, 0xFFu, 0xFFu};
    /* Frame with a 3-byte tag (field 3). */
    static const uint8_t short_tag[] = {0x08u, 0x01u, 0x1Au, 0x03u, 0x01u, 0x02u, 0x03u};
    uint8_t big[CGW_WS_FRAME_MAX + 1u] = {0u};
    cgw_proto_frame_t fr;
    locksys_app_v1_Body body = locksys_app_v1_Body_init_zero;

    TEST_ASSERT_EQUAL(CGW_E_PROTO, cgw_proto_decode_frame(garbage, sizeof(garbage), &fr));
    TEST_ASSERT_EQUAL(CGW_E_PROTO, cgw_proto_decode_frame(short_tag, sizeof(short_tag), &fr));
    TEST_ASSERT_EQUAL(CGW_E_ARG, cgw_proto_decode_frame(big, sizeof(big), &fr));
    TEST_ASSERT_EQUAL(CGW_E_ARG, cgw_proto_decode_frame(NULL, 1u, &fr));
    TEST_ASSERT_EQUAL(CGW_E_PROTO, cgw_proto_decode_body(garbage, sizeof(garbage), &body));
    TEST_ASSERT_EQUAL(CGW_E_ARG, cgw_proto_decode_body(NULL, 1u, &body));
    TEST_ASSERT_EQUAL(CGW_OK, cgw_proto_decode_body(NULL, 0u, &body));
    TEST_ASSERT_EQUAL(0, body.which_msg);
    TEST_ASSERT_FALSE(cgw_proto_body_is_valid(NULL));
    body.which_msg = locksys_app_v1_Body_door_command_tag;
    body.msg.door_command.action = (locksys_app_v1_DoorAction)7;
    TEST_ASSERT_FALSE(cgw_proto_body_is_valid(&body));
    body.which_msg = locksys_app_v1_Body_window_move_tag;
    body.msg.window_move.direction = (locksys_app_v1_WindowDirection)9;
    TEST_ASSERT_FALSE(cgw_proto_body_is_valid(&body));
    body.which_msg = locksys_app_v1_Body_status_update_tag;
    TEST_ASSERT_TRUE(cgw_proto_body_is_valid(&body));
    TEST_ASSERT_FALSE(cgw_proto_is_app_message(locksys_app_v1_Body_status_update_tag));
}

void test_encode_errors(void)
{
    uint8_t out[4];
    size_t len = 0u;
    cgw_proto_frame_t fr;
    locksys_app_v1_Body body = locksys_app_v1_Body_init_zero;

    (void)memset(&fr, 0, sizeof(fr));
    fr.counter = 1u;
    fr.body_len = 10u;
    TEST_ASSERT_EQUAL(CGW_E_PROTO, cgw_proto_encode_frame(&fr, out, sizeof(out), &len));
    TEST_ASSERT_EQUAL(CGW_E_ARG, cgw_proto_encode_frame(NULL, out, sizeof(out), &len));
    body.which_msg = locksys_app_v1_Body_notice_tag;
    (void)strcpy(body.msg.notice.text, "Keep-alive timeout");
    TEST_ASSERT_EQUAL(CGW_E_PROTO, cgw_proto_encode_body(&body, out, sizeof(out), &len));
    TEST_ASSERT_EQUAL(CGW_E_ARG, cgw_proto_encode_body(NULL, out, sizeof(out), &len));
}
