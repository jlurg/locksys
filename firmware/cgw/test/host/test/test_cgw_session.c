/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/*
 * cgw_session host tests. Byte vectors are copied from interfaces/vectors/app_session_v1.json
 * (LS-SAIC-001 v0.2 sections 8.2, 8.3 and 8.6).
 */

#include <string.h>

#include "cgw_proto/cgw_proto.h"
#include "cgw_session/cgw_session.h"
#include "fake_ports.h"
#include "ls_enums_gen.h"
#include "ls_params_gen.h"
#include "soft_crypto.h"
#include "unity.h"

TEST_SOURCE_FILE("pb_common.c")
TEST_SOURCE_FILE("pb_decode.c")
TEST_SOURCE_FILE("pb_encode.c")
TEST_SOURCE_FILE("locksys_app.pb.c")

#define CONN   ((cgw_conn_t)0x0301u)
#define CONN_B ((cgw_conn_t)0x0402u)

static const char *const V_KPAIR =
    "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
static const char *const V_DEVICE_ID = "0102030405060708";
static const char *const V_SERVER_NONCE = "101112131415161718191a1b1c1d1e1f";
static const char *const V_SERVER_PROOF =
    "4a1567ac13dc8410db9924ea4a6d68f93a9b73e2ad1ca5d3ccca2dedc5f22dc4";
static const char *const V_HELLO_BODY =
    "0a370a020801120801020304050607081a10101112131415161718191a1b1c1d1e1f2211302e312e302d6465762b"
    "613162326333642a020801";
static const char *const V_CLIENT_AUTH_FRAME =
    "125312510a0208011210303132333435363738393a3b3c3d3e3f1a10202122232425262728292a2b2c2d2e2f2220"
    "1160faa48fbf464cb8cd77f1d0408080f735e4554fbd8c8397120ae618950f992a05302e312e30";
static const char *const V_AUTH_RESULT_FRAME =
    "080112341a32080112204a1567ac13dc8410db9924ea4a6d68f93a9b73e2ad1ca5d3ccca2dedc5f22dc418f8acd1"
    "910120b817286430de021a1010bffe38f4667e06dccd9446aae43d87";
static const char *const V_PING_FRAME = "08021205220308e8071a10e181c70bd5abaf865929121059a9bda3";
static const char *const V_MOVE_FIRST_FRAME =
    "080112065a04080110011a109a8bb35b86289aaa442691d8d8ef5b19";
static const char *const V_MOVE_KA_FRAME =
    "080212085a060801100118641a10dc652e49d4a56b55874e2762568dd7e8";

static cgw_session_table_t t;
static uint8_t kpair[CGW_KEY_LEN];
static uint8_t device_id[CGW_DEVICE_ID_LEN];

static cgw_proto_frame_t frame_of(const char *hex)
{
    uint8_t buf[CGW_WS_FRAME_MAX];
    size_t len = fake_hex(hex, buf, sizeof(buf));
    cgw_proto_frame_t fr;

    TEST_ASSERT_EQUAL(CGW_OK, cgw_proto_decode_frame(buf, len, &fr));
    return fr;
}

static void open_and_hello(cgw_conn_t conn, uint32_t t_ms)
{
    uint8_t nonce[CGW_NONCE_LEN];
    cgw_ses_identity_t id = {device_id, "0.1.0-dev+a1b2c3d", false};
    locksys_app_v1_ServerHello hello;

    (void)fake_hex(V_SERVER_NONCE, nonce, sizeof(nonce));
    fake_rng_script(nonce, sizeof(nonce));
    TEST_ASSERT_EQUAL(CGW_OK, cgw_session_on_tcp_open(&t, conn, t_ms));
    TEST_ASSERT_EQUAL(CGW_OK, cgw_session_on_ws_open(&t, conn));
    TEST_ASSERT_EQUAL(CGW_OK, cgw_session_build_hello(&t, conn, &id, &hello));
}

static cgw_ses_auth_t authenticate(cgw_conn_t conn, const uint8_t *current, uint32_t t_ms)
{
    static const uint8_t sid[4] = {0x12u, 0x34u, 0x56u, 0x78u};
    cgw_proto_frame_t fr = frame_of(V_CLIENT_AUTH_FRAME);
    locksys_app_v1_Body body = locksys_app_v1_Body_init_zero;
    cgw_ses_auth_input_t in = {device_id, NULL, current};
    cgw_ses_verdict_t v = cgw_session_on_frame(&t, conn, &fr, t_ms);

    TEST_ASSERT_EQUAL(CGW_SES_HANDSHAKE, v.action);
    TEST_ASSERT_EQUAL(CGW_OK, cgw_proto_decode_body(fr.body, fr.body_len, &body));
    TEST_ASSERT_EQUAL(locksys_app_v1_Body_client_auth_tag, body.which_msg);
    fake_rng_script(sid, sizeof(sid));
    return cgw_session_on_client_auth(&t, conn, &body.msg.client_auth, &in, t_ms);
}

static void encode_sealed(const locksys_app_v1_Body *body, cgw_conn_t conn, uint8_t *msg,
                          size_t *len)
{
    uint8_t raw[CGW_PROTO_BODY_MAX];
    size_t raw_len = 0u;
    cgw_proto_frame_t fr;

    TEST_ASSERT_EQUAL(CGW_OK, cgw_proto_encode_body(body, raw, sizeof(raw), &raw_len));
    TEST_ASSERT_EQUAL(CGW_OK, cgw_session_seal(&t, conn, raw, raw_len, &fr));
    TEST_ASSERT_EQUAL(CGW_OK, cgw_proto_encode_frame(&fr, msg, CGW_WS_FRAME_MAX, len));
}

void setUp(void)
{
    fake_ports_reset();
    soft_crypto_reset();
    cgw_session_init(&t);
    (void)fake_hex(V_KPAIR, kpair, sizeof(kpair));
    (void)fake_hex(V_DEVICE_ID, device_id, sizeof(device_id));
}

void tearDown(void)
{
}

/* @verifies SWR-CGW-005 */
void test_server_hello_matches_vector(void)
{
    uint8_t nonce[CGW_NONCE_LEN];
    uint8_t expect[CGW_WS_FRAME_MAX];
    uint8_t raw[CGW_PROTO_BODY_MAX];
    size_t raw_len = 0u;
    size_t n = fake_hex(V_HELLO_BODY, expect, sizeof(expect));
    cgw_ses_identity_t id = {device_id, "0.1.0-dev+a1b2c3d", false};
    locksys_app_v1_Body body = locksys_app_v1_Body_init_zero;

    (void)fake_hex(V_SERVER_NONCE, nonce, sizeof(nonce));
    fake_rng_script(nonce, sizeof(nonce));
    TEST_ASSERT_EQUAL(CGW_OK, cgw_session_on_tcp_open(&t, CONN, 0u));
    TEST_ASSERT_EQUAL(CGW_E_STATE, cgw_session_build_hello(&t, CONN, &id, &body.msg.server_hello));
    TEST_ASSERT_EQUAL(CGW_OK, cgw_session_on_ws_open(&t, CONN));
    body.which_msg = locksys_app_v1_Body_server_hello_tag;
    TEST_ASSERT_EQUAL(CGW_OK, cgw_session_build_hello(&t, CONN, &id, &body.msg.server_hello));
    TEST_ASSERT_EQUAL(CGW_OK, cgw_proto_encode_body(&body, raw, sizeof(raw), &raw_len));
    TEST_ASSERT_EQUAL_size_t(n, raw_len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expect, raw, n);
}

void test_server_hello_without_entropy_fails(void)
{
    cgw_ses_identity_t id = {device_id, "x", false};
    locksys_app_v1_ServerHello hello;

    fake.rng_enabled = false;
    TEST_ASSERT_EQUAL(CGW_OK, cgw_session_on_tcp_open(&t, CONN, 0u));
    TEST_ASSERT_EQUAL(CGW_OK, cgw_session_on_ws_open(&t, CONN));
    TEST_ASSERT_EQUAL(CGW_E_NO_ENTROPY, cgw_session_build_hello(&t, CONN, &id, &hello));
    TEST_ASSERT_EQUAL(CGW_E_ARG, cgw_session_build_hello(&t, CONN, NULL, &hello));
}

/* @verifies SWR-CGW-005 */
void test_client_auth_vector_authenticates(void)
{
    uint8_t proof[CGW_HMAC_LEN];
    cgw_ses_auth_t auth;

    open_and_hello(CONN, 100u);
    auth = authenticate(CONN, kpair, 200u);
    (void)fake_hex(V_SERVER_PROOF, proof, sizeof(proof));
    TEST_ASSERT_EQUAL(LS_COMMAND_RESULT_OK, auth.result);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(proof, auth.server_proof, sizeof(proof));
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, auth.session_id);
    TEST_ASSERT_EQUAL_HEX32(CGW_CONN_NONE, auth.preempted);
    TEST_ASSERT_EQUAL_HEX32(CONN, cgw_session_controller(&t));
    /* K_sess only: the imported K_pair proof key is destroyed. */
    TEST_ASSERT_EQUAL_UINT32(1u, soft_crypto_live_keys());
}

/* @verifies SWR-CGW-006 */
void test_sealed_auth_result_and_ping_match_vectors(void)
{
    uint8_t expect[CGW_WS_FRAME_MAX];
    uint8_t msg[CGW_WS_FRAME_MAX];
    size_t len = 0u;
    size_t n;
    cgw_ses_auth_t auth;
    locksys_app_v1_Body body = locksys_app_v1_Body_init_zero;

    open_and_hello(CONN, 0u);
    auth = authenticate(CONN, kpair, 0u);
    body.which_msg = locksys_app_v1_Body_auth_result_tag;
    body.msg.auth_result.result = locksys_app_v1_CommandResult_COMMAND_RESULT_OK;
    (void)memcpy(body.msg.auth_result.server_proof, auth.server_proof, CGW_HMAC_LEN);
    body.msg.auth_result.session_id = auth.session_id;
    body.msg.auth_result.session_timeout_ms = LS_T_SESSION_TO_MS;
    body.msg.auth_result.keepalive_period_ms = LS_T_APP_KA_MS;
    body.msg.auth_result.keepalive_timeout_ms = LS_T_CGW_KA_TO_MS;
    encode_sealed(&body, CONN, msg, &len);
    n = fake_hex(V_AUTH_RESULT_FRAME, expect, sizeof(expect));
    TEST_ASSERT_EQUAL_size_t(n, len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expect, msg, n);

    (void)memset(&body, 0, sizeof(body));
    body.which_msg = locksys_app_v1_Body_ping_tag;
    body.msg.ping.timestamp_ms = 1000u;
    encode_sealed(&body, CONN, msg, &len);
    n = fake_hex(V_PING_FRAME, expect, sizeof(expect));
    TEST_ASSERT_EQUAL_size_t(n, len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expect, msg, n);
}

/* @verifies SWR-CGW-006 */
void test_app_frames_delivered_and_replay_closes_1008(void)
{
    cgw_proto_frame_t first = frame_of(V_MOVE_FIRST_FRAME);
    cgw_proto_frame_t ka = frame_of(V_MOVE_KA_FRAME);
    cgw_ses_verdict_t v;

    open_and_hello(CONN, 0u);
    (void)authenticate(CONN, kpair, 0u);
    v = cgw_session_on_frame(&t, CONN, &first, 10u);
    TEST_ASSERT_EQUAL(CGW_SES_DELIVER, v.action);
    v = cgw_session_on_frame(&t, CONN, &ka, 110u);
    TEST_ASSERT_EQUAL(CGW_SES_DELIVER, v.action);
    TEST_ASSERT_EQUAL_UINT32(2u, cgw_session_find(&t, CONN)->rx_counter);
    TEST_ASSERT_EQUAL_UINT32(110u, cgw_session_find(&t, CONN)->t_last_rx_ms);
    v = cgw_session_on_frame(&t, CONN, &first, 120u);
    TEST_ASSERT_EQUAL(CGW_SES_CLOSE, v.action);
    TEST_ASSERT_EQUAL_UINT16(CGW_CLOSE_POLICY, v.close_code);
    TEST_ASSERT_TRUE(v.controller_lost);
    TEST_ASSERT_EQUAL_UINT32(1u, t.stats.replays);
    /* Frames of a closing session are dropped. */
    v = cgw_session_on_frame(&t, CONN, &ka, 130u);
    TEST_ASSERT_EQUAL(CGW_SES_DROP, v.action);
}

/* @verifies SWR-CGW-006 */
void test_tag_with_wrong_direction_closes(void)
{
    cgw_proto_frame_t fr = frame_of(V_MOVE_FIRST_FRAME);
    cgw_ses_verdict_t v;

    open_and_hello(CONN, 0u);
    (void)authenticate(CONN, kpair, 0u);
    /* Negative vector tag_with_wrong_direction: body tagged with dir 0x43. */
    (void)fake_hex("4f792c9bbf369001b40ed5b2c67ed10c", fr.tag, sizeof(fr.tag));
    v = cgw_session_on_frame(&t, CONN, &fr, 10u);
    TEST_ASSERT_EQUAL(CGW_SES_CLOSE, v.action);
    TEST_ASSERT_EQUAL_UINT16(CGW_CLOSE_POLICY, v.close_code);
    TEST_ASSERT_EQUAL_UINT32(1u, t.stats.bad_tags);
}

/* @verifies SWR-CGW-006 */
void test_counter_is_valid(void)
{
    cgw_session_t s;

    (void)memset(&s, 0, sizeof(s));
    TEST_ASSERT_FALSE(cgw_session_counter_is_valid(&s, 0u));
    TEST_ASSERT_TRUE(cgw_session_counter_is_valid(&s, 1u));
    s.rx_counter = 5u;
    TEST_ASSERT_FALSE(cgw_session_counter_is_valid(&s, 4u));
    TEST_ASSERT_FALSE(cgw_session_counter_is_valid(&s, 5u));
    TEST_ASSERT_TRUE(cgw_session_counter_is_valid(&s, 6u));
    TEST_ASSERT_TRUE(cgw_session_counter_is_valid(&s, UINT32_MAX));
    TEST_ASSERT_FALSE(cgw_session_counter_is_valid(NULL, 6u));
}

/* @verifies SWR-CGW-011 */
void test_rate_limit_rolling_window(void)
{
    cgw_rate_window_t w;

    (void)memset(&w, 0, sizeof(w));
    for (uint32_t i = 0u; i < LS_N_RATE_LIMIT_FRAMES; i++)
    {
        TEST_ASSERT_TRUE(cgw_session_rate_admit(&w, 1000u + i));
    }
    TEST_ASSERT_FALSE(cgw_session_rate_admit(&w, 1000u + LS_T_RATE_WINDOW_MS - 1u));
    TEST_ASSERT_TRUE(cgw_session_rate_admit(&w, 1000u + LS_T_RATE_WINDOW_MS));
    TEST_ASSERT_FALSE(cgw_session_rate_admit(NULL, 0u));
}

/* @verifies SWR-CGW-005 */
void test_handshake_timeout_closes(void)
{
    cgw_ses_tick_result_t r;

    TEST_ASSERT_EQUAL(CGW_OK, cgw_session_on_tcp_open(&t, CONN, 0u));
    open_and_hello(CONN_B, 0u);
    cgw_session_tick(&t, LS_T_CGW_HANDSHAKE_TO_MS - 1u, &r);
    TEST_ASSERT_EQUAL_UINT8(0u, r.count);
    cgw_session_tick(&t, LS_T_CGW_HANDSHAKE_TO_MS, &r);
    TEST_ASSERT_EQUAL_UINT8(2u, r.count);
    TEST_ASSERT_EQUAL_HEX32(CONN, r.closes[0].conn);
    TEST_ASSERT_EQUAL_UINT16(CGW_CLOSE_NONE, r.closes[0].close_code);
    TEST_ASSERT_EQUAL_HEX32(CONN_B, r.closes[1].conn);
    TEST_ASSERT_EQUAL_UINT16(CGW_CLOSE_HANDSHAKE_TIMEOUT, r.closes[1].close_code);
    TEST_ASSERT_EQUAL_UINT32(2u, t.stats.auth_failures);
}

/* @verifies SWR-CGW-010 */
void test_session_timeout_closes_4004(void)
{
    cgw_ses_tick_result_t r;

    open_and_hello(CONN, 0u);
    (void)authenticate(CONN, kpair, 100u);
    cgw_session_tick(&t, 100u + LS_T_SESSION_TO_MS - 1u, &r);
    TEST_ASSERT_EQUAL_UINT8(0u, r.count);
    cgw_session_tick(&t, 100u + LS_T_SESSION_TO_MS, &r);
    TEST_ASSERT_EQUAL_UINT8(1u, r.count);
    TEST_ASSERT_EQUAL_UINT16(CGW_CLOSE_SESSION_TIMEOUT, r.closes[0].close_code);
    TEST_ASSERT_TRUE(r.closes[0].controller_lost);
    TEST_ASSERT_TRUE(r.closes[0].send_session_close);
    TEST_ASSERT_TRUE(cgw_session_on_close(&t, CONN) == false);
    TEST_ASSERT_EQUAL_UINT32(0u, soft_crypto_live_keys());
}

/* @verifies SWR-CGW-007 */
/* @verifies SWR-CGW-008 */
void test_failed_auth_and_throttling(void)
{
    uint8_t wrong[CGW_KEY_LEN] = {0u};
    cgw_ses_auth_t auth;

    for (uint32_t i = 0u; i < LS_N_AUTH_FAIL_THROTTLE; i++)
    {
        cgw_conn_t conn = (cgw_conn_t)(0x500u + i);

        open_and_hello(conn, 1000u + i);
        auth = authenticate(conn, wrong, 1000u + i);
        TEST_ASSERT_EQUAL(LS_COMMAND_RESULT_REJECTED_AUTH, auth.result);
        TEST_ASSERT_EQUAL_UINT16(CGW_CLOSE_AUTH_FAILED, auth.close_code);
        TEST_ASSERT_FALSE(cgw_session_on_close(&t, conn));
    }
    TEST_ASSERT_EQUAL_UINT8(LS_N_AUTH_FAIL_THROTTLE, t.failures);
    open_and_hello(CONN_B, 2000u);
    auth = authenticate(CONN_B, kpair, 1000u + LS_T_AUTH_THROTTLE_MS);
    TEST_ASSERT_EQUAL(LS_COMMAND_RESULT_REJECTED_RATE_LIMIT, auth.result);
    TEST_ASSERT_EQUAL_UINT16(CGW_CLOSE_HANDSHAKE_TIMEOUT, auth.close_code);
    TEST_ASSERT_EQUAL_UINT32(1u, t.stats.throttled);
    TEST_ASSERT_FALSE(cgw_session_on_close(&t, CONN_B));
    /* Decay: no failure for t_auth_fail_decay_ms. */
    open_and_hello(CONN_B, 1002u + LS_T_AUTH_FAIL_DECAY_MS);
    auth = authenticate(CONN_B, kpair, 1002u + LS_T_AUTH_FAIL_DECAY_MS);
    TEST_ASSERT_EQUAL(LS_COMMAND_RESULT_OK, auth.result);
}

void test_auth_without_pairing_and_version_mismatch(void)
{
    cgw_ses_auth_t auth;
    locksys_app_v1_ClientAuth ca;
    cgw_ses_auth_input_t in = {device_id, NULL, NULL};

    open_and_hello(CONN, 0u);
    auth = authenticate(CONN, NULL, 0u);
    TEST_ASSERT_EQUAL(LS_COMMAND_RESULT_UNSPECIFIED, auth.result);
    TEST_ASSERT_EQUAL_UINT16(CGW_CLOSE_PAIRING_REQUIRED, auth.close_code);

    (void)memset(&ca, 0, sizeof(ca));
    open_and_hello(CONN_B, 0u);
    auth = cgw_session_on_client_auth(&t, CONN_B, &ca, &in, 0u);
    TEST_ASSERT_EQUAL(LS_COMMAND_RESULT_REJECTED_VERSION, auth.result);
    TEST_ASSERT_EQUAL_UINT16(CGW_CLOSE_VERSION, auth.close_code);
}

/* @verifies SWR-CGW-009 */
void test_single_controller_busy_and_preemption(void)
{
    cgw_ses_auth_t auth;

    open_and_hello(CONN, 0u);
    (void)authenticate(CONN, kpair, 0u);
    /* Same client_id: pre-empts the controller. */
    open_and_hello(CONN_B, 100u);
    auth = authenticate(CONN_B, kpair, 100u);
    TEST_ASSERT_EQUAL(LS_COMMAND_RESULT_OK, auth.result);
    TEST_ASSERT_EQUAL_HEX32(CONN, auth.preempted);
    TEST_ASSERT_EQUAL_HEX32(CONN_B, cgw_session_controller(&t));
}

/* @verifies SWR-CGW-009 */
void test_other_client_rejected_busy_while_controller_alive(void)
{
    cgw_ses_auth_t auth;

    open_and_hello(CONN, 0u);
    (void)authenticate(CONN, kpair, 0u);
    t.entries[0].client_id[0] ^= 0xFFu; /* controller now has another client_id */
    open_and_hello(CONN_B, 100u);
    auth = authenticate(CONN_B, kpair, 100u);
    TEST_ASSERT_EQUAL(LS_COMMAND_RESULT_REJECTED_BUSY, auth.result);
    TEST_ASSERT_EQUAL_UINT16(CGW_CLOSE_BUSY, auth.close_code);
}

/* @verifies SWR-CGW-023 */
void test_ping_schedule_and_link_quality(void)
{
    open_and_hello(CONN, 0u);
    TEST_ASSERT_FALSE(cgw_session_ping_due(&t, 0u, false));
    (void)authenticate(CONN, kpair, 0u);
    TEST_ASSERT_TRUE(cgw_session_ping_due(&t, 0u, false));
    TEST_ASSERT_FALSE(cgw_session_link_ok(&t, 10u));
    cgw_session_on_pong(&t, CONN, 0u, 40u);
    TEST_ASSERT_TRUE(cgw_session_link_ok(&t, 50u));
    TEST_ASSERT_FALSE(cgw_session_ping_due(&t, 100u, true));
    TEST_ASSERT_TRUE(cgw_session_ping_due(&t, LS_T_PING_MOTION_MS, true));
    /* No Pong within t_pong_to_ms: the link gate fails. */
    TEST_ASSERT_FALSE(cgw_session_link_ok(&t, LS_T_PING_MOTION_MS + LS_T_PONG_TO_MS));
    /* RTT above the limit fails the gate. */
    cgw_session_on_pong(&t, CONN, LS_T_PING_MOTION_MS, LS_T_PING_MOTION_MS + LS_T_RTT_MAX_MS + 1u);
    TEST_ASSERT_FALSE(cgw_session_link_ok(&t, LS_T_PING_MOTION_MS + LS_T_RTT_MAX_MS + 2u));
}

void test_table_full_and_violation(void)
{
    cgw_ses_verdict_t v;

    for (uint32_t i = 0u; i < CGW_SESSIONS_MAX; i++)
    {
        TEST_ASSERT_EQUAL(CGW_OK, cgw_session_on_tcp_open(&t, (cgw_conn_t)(0x100u + i), 0u));
    }
    TEST_ASSERT_EQUAL(CGW_E_FULL, cgw_session_on_tcp_open(&t, 0x200u, 0u));
    TEST_ASSERT_EQUAL(CGW_E_ARG, cgw_session_on_tcp_open(&t, 0x100u, 0u));
    TEST_ASSERT_EQUAL_UINT32(1u, t.stats.refused);
    v = cgw_session_on_violation(&t, 0x100u, 5u);
    TEST_ASSERT_EQUAL(CGW_SES_CLOSE, v.action);
    TEST_ASSERT_EQUAL_UINT16(CGW_CLOSE_POLICY, v.close_code);
    v = cgw_session_on_violation(&t, 0x999u, 5u);
    TEST_ASSERT_EQUAL(CGW_SES_DROP, v.action);
}
