/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file mode_mgr_priv.h
 * @brief ModeMgr internals: events, guards and actions.
 *
 * Included only by the files of src/app/mode_mgr/ and by their unit tests.
 */

#ifndef MODE_MGR_PRIV_H
#define MODE_MGR_PRIV_H

#include "ls_common/ls_evset.h"
#include "rte/rte_mode_mgr.h"

/* Event bits, drained lowest first (LS-DCU-SAD-001 section 8.4). */
#define MODE_MGR_EV_SAFE_REQ       ((LsEvSet_EventType)0u) /**< SAFE latch or critical cause */
#define MODE_MGR_EV_TM_INIT_MAX    ((LsEvSet_EventType)1u) /**< INIT deadline */
#define MODE_MGR_EV_NON_CRIT_FAULT ((LsEvSet_EventType)2u) /**< DEGRADED-severity DTC active */
#define MODE_MGR_EV_HEALED_NOW     ((LsEvSet_EventType)3u) /**< Last DEGRADED DTC inactive */
#define MODE_MGR_EV_TM_HEAL        ((LsEvSet_EventType)4u) /**< Heal time elapsed */
#define MODE_MGR_EV_SELF_TEST_OK   ((LsEvSet_EventType)5u) /**< INIT completion conditions hold */

/** @brief Drain budget per cycle. */
#define MODE_MGR_DRAIN_BUDGET (6u)

/**
 * @brief evSafeReq condition: the SAFE latch is set or a critical cause is active.
 *
 * @param[in] safeLatched    SAFE latch set.
 * @param[in] criticalActive A CRITICAL DTC or an escalation is active.
 * @return true when SAFE is requested.
 */
bool ModeMgr_GuardSafeRequest(bool safeLatched, bool criticalActive);

/**
 * @brief Action: records the NodeMode entered.
 *
 * @param[in] mode NodeMode.
 */
void ModeMgr_ActSetMode(Ls_NodeModeType mode);

/**
 * @brief Returns the NodeMode recorded by the actions.
 *
 * @return NodeMode.
 */
Ls_NodeModeType ModeMgr_ActionsMode(void);

#endif /* MODE_MGR_PRIV_H */
