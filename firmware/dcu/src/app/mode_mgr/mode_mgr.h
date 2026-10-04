/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file mode_mgr.h
 * @brief ModeMgr SWC: NodeMode statechart and the inhibit computation.
 */

#ifndef MODE_MGR_H
#define MODE_MGR_H

/**
 * @brief Initialises the adapter and the engine; NodeMode starts in INIT.
 */
void ModeMgr_Init(void);

/**
 * @brief Runs one adapter cycle.
 *
 * @note 10 ms task; the only caller of the ModeMgr engine.
 */
void ModeMgr_Main10ms(void);

#endif /* MODE_MGR_H */
