/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file ls_cfg.h
 * @brief Build switches of the DCU firmware (LS-DCU-SAD-001 section 14).
 *
 * The build system defines the switches per configuration (Debug, Hil, Release). A switch
 * that is not defined takes the Release value, so an incomplete build definition never
 * enables test features.
 */

#ifndef LS_CFG_H
#define LS_CFG_H

#include "platform/ls_static_assert.h"

#ifndef LS_CFG_DET
/** @brief 1: development error reporting (Det) is compiled in. */
#define LS_CFG_DET 0
#endif

#ifndef LS_CFG_FI
/** @brief 1: fault injection (Fi, `!LSFI`) and USART2 RX are compiled in. */
#define LS_CFG_FI 0
#endif

#ifndef LS_CFG_TRACE
/** @brief 1: trace pins TRACE0-TRACE3 are driven. */
#define LS_CFG_TRACE 0
#endif

#ifndef LS_CFG_TRCOV
/** @brief 1: the transition coverage bitmap of DID 0xFD09 is maintained. */
#define LS_CFG_TRCOV 0
#endif

#ifndef LS_CFG_ROMCRC_ENFORCE
/** @brief 1: a ROM CRC mismatch at start-up enters SAFE; 0: it is reported only. */
#define LS_CFG_ROMCRC_ENFORCE 1
#endif

#ifndef LS_CFG_VS_ENGINE_PRESENT
/** @brief 1: the Visual State engines of gen_vs/ are linked and called by the SWC adapters. */
#define LS_CFG_VS_ENGINE_PRESENT 0
#endif

#ifndef LS_CFG_LOCK_DIAG_OPTION_B
/** @brief 1: lock EN/DIAG on PB4 (option B, recommended); 0: on PA6 (option A). Open point O15. */
#define LS_CFG_LOCK_DIAG_OPTION_B 1
#endif

LS_STATIC_ASSERT((LS_CFG_DET == 0) || (LS_CFG_DET == 1), "LS_CFG_DET must be 0 or 1");
LS_STATIC_ASSERT((LS_CFG_FI == 0) || (LS_CFG_FI == 1), "LS_CFG_FI must be 0 or 1");
LS_STATIC_ASSERT((LS_CFG_TRACE == 0) || (LS_CFG_TRACE == 1), "LS_CFG_TRACE must be 0 or 1");
LS_STATIC_ASSERT((LS_CFG_TRCOV == 0) || (LS_CFG_TRCOV == 1), "LS_CFG_TRCOV must be 0 or 1");
LS_STATIC_ASSERT((LS_CFG_ROMCRC_ENFORCE == 0) || (LS_CFG_ROMCRC_ENFORCE == 1),
                 "LS_CFG_ROMCRC_ENFORCE must be 0 or 1");
LS_STATIC_ASSERT((LS_CFG_VS_ENGINE_PRESENT == 0) || (LS_CFG_VS_ENGINE_PRESENT == 1),
                 "LS_CFG_VS_ENGINE_PRESENT must be 0 or 1");
LS_STATIC_ASSERT((LS_CFG_LOCK_DIAG_OPTION_B == 0) || (LS_CFG_LOCK_DIAG_OPTION_B == 1),
                 "LS_CFG_LOCK_DIAG_OPTION_B must be 0 or 1");

#endif /* LS_CFG_H */
