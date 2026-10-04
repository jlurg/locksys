/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file cgw_store_nvs.h
 * @brief Key store adapter over NVS (cgw_keystore_port.h), namespace cgw_sec.
 *
 * MVP: plain NVS on the development board; HMAC-based NVS encryption is LATER (LS-SAIC-001
 * section 12). Writes stall both CPUs during a flash erase, so the core writes only while no
 * press is active.
 */

#ifndef CGW_STORE_NVS_H
#define CGW_STORE_NVS_H

#include "cgw_ports/cgw_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
 * @brief Initialise the default NVS partition; a partition without free pages or with a new
 *        format version is erased first.
 *
 * @retval CGW_OK   NVS ready.
 * @retval CGW_E_IO NVS could not be initialised (B1B15).
 */
    cgw_rc_t cgw_store_nvs_init(void);

#ifdef __cplusplus
}
#endif

#endif /* CGW_STORE_NVS_H */
