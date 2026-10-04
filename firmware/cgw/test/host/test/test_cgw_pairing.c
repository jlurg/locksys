/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/* cgw_pairing host tests: window, commit, payload format, passphrase, SSID. */

#include <string.h>

#include "cgw_pairing/cgw_pairing.h"
#include "fake_ports.h"
#include "ls_params_gen.h"
#include "soft_crypto.h"
#include "unity.h"

static cgw_pairing_t p;
static uint8_t kpair[CGW_KEY_LEN];

void setUp(void)
{
    fake_ports_reset();
    cgw_pairing_init(&p);
    for (size_t i = 0u; i < sizeof(kpair); i++)
    {
        kpair[i] = (uint8_t)i;
    }
}

void tearDown(void)
{
}

/* @verifies SWR-CGW-016 */
void test_window_opens_with_rng_key_and_expires(void)
{
    fake.rng_enabled = false;
    TEST_ASSERT_EQUAL(CGW_E_NO_ENTROPY, cgw_pairing_open(&p, 0u));
    TEST_ASSERT_NULL(cgw_pairing_pending_key(&p));
    fake.rng_enabled = true;
    fake_rng_script(kpair, sizeof(kpair));
    TEST_ASSERT_EQUAL(CGW_OK, cgw_pairing_open(&p, 100u));
    TEST_ASSERT_EQUAL(CGW_E_STATE, cgw_pairing_open(&p, 100u));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(kpair, cgw_pairing_pending_key(&p), CGW_KEY_LEN);
    TEST_ASSERT_FALSE(cgw_pairing_tick(&p, 100u + LS_T_PAIRING_WINDOW_MS - 1u));
    TEST_ASSERT_TRUE(cgw_pairing_tick(&p, 100u + LS_T_PAIRING_WINDOW_MS));
    TEST_ASSERT_FALSE(cgw_pairing_is_open(&p));
    TEST_ASSERT_EACH_EQUAL_HEX8(0u, p.pending, CGW_KEY_LEN);
}

/* @verifies SWR-CGW-018 */
void test_commit_stores_record_and_closes(void)
{
    uint8_t cid[CGW_CLIENT_ID_LEN] = {9u};
    cgw_pairing_record_t rec;

    (void)memset(&rec, 0, sizeof(rec));
    rec.generation = 4u;
    TEST_ASSERT_EQUAL(CGW_E_STATE, cgw_pairing_commit(&p, cid, &rec));
    fake_rng_script(kpair, sizeof(kpair));
    (void)cgw_pairing_open(&p, 0u);
    TEST_ASSERT_EQUAL(CGW_OK, cgw_pairing_commit(&p, cid, &rec));
    TEST_ASSERT_TRUE(fake.ks_has_rec);
    TEST_ASSERT_EQUAL_UINT32(5u, fake.ks_rec.generation);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(kpair, fake.ks_rec.k_pair, CGW_KEY_LEN);
    TEST_ASSERT_EQUAL_UINT8(9u, fake.ks_rec.client_id[0]);
    TEST_ASSERT_FALSE(cgw_pairing_is_open(&p));
    fake.ks_rc = CGW_E_IO;
    (void)cgw_pairing_open(&p, 0u);
    TEST_ASSERT_EQUAL(CGW_E_IO, cgw_pairing_commit(&p, cid, &rec));
    TEST_ASSERT_FALSE(cgw_pairing_is_open(&p));
}

/* @verifies SWR-CGW-017 */
void test_pairing_uri_format(void)
{
    static const uint8_t dev[CGW_DEVICE_ID_LEN] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    static const uint8_t mac[CGW_MAC_LEN] = {0x7Cu, 0xDFu, 0xA1u, 0x00u, 0xAAu, 0xBBu};
    char ssid[CGW_SSID_SIZE];
    char uri[CGW_PAIR_URI_MAX];
    cgw_pair_uri_in_t in = {dev, ssid, "ABCDEFGHIJKLMNOPQRST", kpair, mac, false};
    size_t len;

    cgw_pairing_ssid(mac, ssid);
    TEST_ASSERT_EQUAL_STRING("LockSys-AABB", ssid);
    len = cgw_pairing_build_uri(&in, uri, sizeof(uri));
    TEST_ASSERT_EQUAL_STRING("locksys://pair?v=1&id=0102030405060708&s=LockSys-AABB"
                             "&p=ABCDEFGHIJKLMNOPQRST"
                             "&k=AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8"
                             "&b=7C:DF:A1:00:AA:BB&sec=wpa3",
                             uri);
    TEST_ASSERT_EQUAL_size_t(strlen(uri), len);
    in.transition = true;
    len = cgw_pairing_build_uri(&in, uri, sizeof(uri));
    TEST_ASSERT_EQUAL_STRING("&sec=wpa2wpa3", &uri[len - 13u]);
    TEST_ASSERT_EQUAL_size_t(0u, cgw_pairing_build_uri(&in, uri, 40u));
    TEST_ASSERT_EQUAL_CHAR('\0', uri[0]);
    in.k_pair = NULL;
    TEST_ASSERT_EQUAL_size_t(0u, cgw_pairing_build_uri(&in, uri, sizeof(uri)));
}

void test_base64url(void)
{
    static const uint8_t in[] = {0xFBu, 0xFFu};
    char out[8];

    TEST_ASSERT_EQUAL_size_t(3u, cgw_pairing_base64url(in, sizeof(in), out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("-_8", out);
    TEST_ASSERT_EQUAL_size_t(0u, cgw_pairing_base64url(in, sizeof(in), out, 3u));
    TEST_ASSERT_EQUAL_size_t(0u, cgw_pairing_base64url(NULL, 1u, out, sizeof(out)));
}

/* @verifies SWR-CGW-002 */
void test_passphrase_base32_from_rng(void)
{
    static const uint8_t rnd[4] = {0x00u, 0x1Fu, 0x20u, 0xFAu};
    char pass[CGW_PASSPHRASE_LEN + 1u];

    fake_rng_script(rnd, sizeof(rnd));
    TEST_ASSERT_EQUAL(CGW_OK, cgw_pairing_make_passphrase(pass, sizeof(pass)));
    TEST_ASSERT_EQUAL_STRING("A7A2A7A2A7A2A7A2A7A2", pass);
    TEST_ASSERT_EQUAL(CGW_E_ARG, cgw_pairing_make_passphrase(pass, CGW_PASSPHRASE_LEN));
    fake.rng_enabled = false;
    TEST_ASSERT_EQUAL(CGW_E_NO_ENTROPY, cgw_pairing_make_passphrase(pass, sizeof(pass)));
}
