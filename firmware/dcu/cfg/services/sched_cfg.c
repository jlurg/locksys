/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file sched_cfg.c
 * @brief Task table and runnable binding (LS-DCU-SAD-001 section 5.1).
 *
 * The only place where task bodies of every layer are bound to the task table.
 */

#include "services/sched/sched.h"

#include "app/cmd_arb/cmd_arb.h"
#include "app/door_ctrl/door_ctrl.h"
#include "app/mode_mgr/mode_mgr.h"
#include "app/temp_mon/temp_mon.h"
#include "app/win_ctrl/win_ctrl.h"
#include "ecual/canif/canif.h"
#include "ecual/digin/digin.h"
#include "ecual/hbridge/hbridge.h"
#include "ecual/lockact/lockact.h"
#include "ecual/vbatmon/vbatmon.h"
#include "ecual/winpos/winpos.h"
#include "ls_cfg.h"
#include "mcal/gpio/gpio.h"
#include "mcal/gpio_cfg.h"
#include "mcal/i2c/i2c.h"
#include "platform/ls_static_assert.h"
#include "services/com/com.h"
#include "services/dcm/dcm.h"
#include "services/dem/dem.h"
#include "services/fi/fi.h"
#include "services/isotp/isotp.h"
#include "services/safemon/safemon.h"
#include "services/tlm/tlm.h"
#include "services/wdgm/wdgm.h"

#define SCHED_CFG_TRACE_TASK   (0x01u) /* TRACE0: high while a task runs */
#define SCHED_CFG_TRACE_REFLEX (0x08u) /* TRACE3: pulse in a reflex handler */

LS_STATIC_ASSERT(SCHED_TASK_T1000 == (SCHED_CFG_TASK_COUNT - 1u), "task table size");
LS_STATIC_ASSERT(WDGM_SE_COUNT == SCHED_CFG_TASK_COUNT, "one supervised entity per task");

const Sched_TaskCfgType SchedCfg_Tasks[SCHED_CFG_TASK_COUNT] = {
    {1u, 0u},    /* T1 */
    {5u, 1u},    /* T5 */
    {10u, 2u},   /* T10 */
    {100u, 4u},  /* T100 */
    {1000u, 8u}, /* T1000 */
};

static void SchedCfg_Heartbeat(void)
{
    const Ls_LevelType level = Gpio_ReadOutput(GPIO_PIN_LD2);

    Gpio_Write(GPIO_PIN_LD2, (level == LS_HIGH) ? LS_LOW : LS_HIGH);
}

static void SchedCfg_RunT10(void)
{
    CanIf_Main10ms();
    Com_MainRx();
    WinPos_Main10ms();
    VbatMon_Main10ms();
    CmdArb_Main10ms();
    ModeMgr_Main10ms();
    WinCtrl_Main10ms();
    DoorCtrl_Main10ms();
    Dcm_Main10ms();
#if LS_CFG_FI == 1
    Fi_Main10ms();
#endif
    Com_MainTx();
    Tlm_Main10ms();
    WdgM_Checkpoint(WDGM_SE_T10);
    WdgM_Main10ms();
}

void SchedCfg_RunTask(Sched_TaskIdType task)
{
    switch (task)
    {
        case SCHED_TASK_T1:
            HBridge_Main1ms();
            LockAct_Main1ms();
            WdgM_Checkpoint(WDGM_SE_T1);
            break;
        case SCHED_TASK_T5:
            DigIn_Main5ms();
            I2c_MainFunction();
            TempMon_Main5ms();
            IsoTp_Main5ms();
            WdgM_Checkpoint(WDGM_SE_T5);
            break;
        case SCHED_TASK_T10:
            SchedCfg_RunT10();
            break;
        case SCHED_TASK_T100:
            Dem_Main100ms();
            SafeMon_Main100ms();
            WdgM_Checkpoint(WDGM_SE_T100);
            break;
        case SCHED_TASK_T1000:
            TempMon_Main1000ms();
            Tlm_Main1000ms();
            SchedCfg_Heartbeat();
            WdgM_Checkpoint(WDGM_SE_T1000);
            break;
        default:
            /* Unknown task identifier: nothing runs. */
            break;
    }
}

void SchedCfg_TaskBegin(Sched_TaskIdType task)
{
    (void)task;
    Gpio_TraceSet(SCHED_CFG_TRACE_TASK, 0u);
}

void SchedCfg_TaskEnd(Sched_TaskIdType task)
{
    (void)task;
    Gpio_TraceSet(0u, SCHED_CFG_TRACE_TASK);
}

void SchedCfg_HangReaction(void)
{
    Gpio_TraceSet(SCHED_CFG_TRACE_REFLEX, 0u);
    HBridge_ReflexStop(HBRIDGE_CH_WIN, HBRIDGE_REFLEX_HANG);
    HBridge_ReflexStop(HBRIDGE_CH_LOCK, HBRIDGE_REFLEX_HANG);
    HBridge_AllOff();
    Gpio_TraceSet(0u, SCHED_CFG_TRACE_REFLEX);
}
