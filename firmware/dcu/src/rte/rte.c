/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file rte.c
 * @brief RTE port buffers, read-through calls and forwarding to Com.
 *
 * Ports are accessed from task context only; tasks never preempt each other, so a structure
 * copy is consistent.
 */

#include "rte/rte.h"

#include "ecual/hbridge/hbridge.h"
#include "ecual/lockact/lockact.h"
#include "platform/ls_static_assert.h"
#include "rte/rte_cmd_arb.h"
#include "rte/rte_door_ctrl.h"
#include "rte/rte_mode_mgr.h"
#include "rte/rte_win_ctrl.h"
#include "services/com/com.h"
#include "services/safemon/safemon.h"
#include "services/tbase/tbase.h"

LS_STATIC_ASSERT(RTE_BRIDGE_OFF == HBRIDGE_CMD_OFF, "bridge command mapping");
LS_STATIC_ASSERT(RTE_BRIDGE_DRIVE_UP == HBRIDGE_CMD_DRIVE_UP, "bridge command mapping");
LS_STATIC_ASSERT(RTE_BRIDGE_DRIVE_DOWN == HBRIDGE_CMD_DRIVE_DOWN, "bridge command mapping");
LS_STATIC_ASSERT(RTE_BRIDGE_BRAKE == HBRIDGE_CMD_BRAKE, "bridge command mapping");
LS_STATIC_ASSERT(RTE_SAFE_CAUSE_MODE == SAFEMON_CAUSE_MODE, "SAFE cause mapping");
LS_STATIC_ASSERT(RTE_SAFE_CAUSE_ENGINE == SAFEMON_CAUSE_ENGINE, "SAFE cause mapping");

static Rte_WinRequestType s_winRequest;
static Rte_DoorRequestType s_doorRequest;
static Rte_CgwStatusType s_cgwStatus;
static Rte_EcuModeType s_ecuMode;
static Rte_WinStatusType s_winStatus;
static Rte_DoorStatusType s_doorStatus;

void Rte_Init(void)
{
    static const Rte_WinRequestType winRequestInit = {
        LS_WINDOW_REQUEST_STOP, 0u, 0u, RTE_FRAME_INVALID, false, 0u, false};
    static const Rte_DoorRequestType doorRequestInit = {LS_DOOR_REQUEST_NONE, 0u, false};
    static const Rte_CgwStatusType cgwStatusInit = {
        LS_NODE_MODE_UNKNOWN, 0u, 0u, false, false, false};
    static const Rte_EcuModeType ecuModeInit = {LS_NODE_MODE_INIT, true, true, true, false};
    static const Rte_WinStatusType winStatusInit = {
        LS_WINDOW_STATE_UNKNOWN, LS_WINDOW_STOP_REASON_NONE, 0u, LS_COMMAND_RESULT_UNSPECIFIED, 0u};
    static const Rte_DoorStatusType doorStatusInit = {LS_DOOR_LOCK_STATE_UNKNOWN, 0u,
                                                      LS_COMMAND_RESULT_UNSPECIFIED, false};

    s_winRequest = winRequestInit;
    s_doorRequest = doorRequestInit;
    s_cgwStatus = cgwStatusInit;
    s_ecuMode = ecuModeInit;
    s_winStatus = winStatusInit;
    s_doorStatus = doorStatusInit;
}

Ls_ReturnType Rte_Read_WinRequest(Rte_WinRequestType *data)
{
    if (data == NULL)
    {
        return LS_E_NOT_OK;
    }
    *data = s_winRequest;
    return LS_E_OK;
}

void Rte_Write_WinRequest(const Rte_WinRequestType *data)
{
    if (data != NULL)
    {
        s_winRequest = *data;
    }
}

Ls_ReturnType Rte_Read_DoorRequest(Rte_DoorRequestType *data)
{
    if (data == NULL)
    {
        return LS_E_NOT_OK;
    }
    *data = s_doorRequest;
    return LS_E_OK;
}

void Rte_Write_DoorRequest(const Rte_DoorRequestType *data)
{
    if (data != NULL)
    {
        s_doorRequest = *data;
    }
}

Ls_ReturnType Rte_Read_CgwStatus(Rte_CgwStatusType *data)
{
    if (data == NULL)
    {
        return LS_E_NOT_OK;
    }
    *data = s_cgwStatus;
    return LS_E_OK;
}

void Rte_Write_CgwStatus(const Rte_CgwStatusType *data)
{
    if (data != NULL)
    {
        s_cgwStatus = *data;
    }
}

Ls_ReturnType Rte_Read_EcuMode(Rte_EcuModeType *data)
{
    if (data == NULL)
    {
        return LS_E_NOT_OK;
    }
    *data = s_ecuMode;
    return LS_E_OK;
}

void Rte_Write_EcuMode(const Rte_EcuModeType *data)
{
    if (data != NULL)
    {
        s_ecuMode = *data;
    }
}

Ls_ReturnType Rte_Read_WinStatus(Rte_WinStatusType *data)
{
    if (data == NULL)
    {
        return LS_E_NOT_OK;
    }
    *data = s_winStatus;
    return LS_E_OK;
}

Ls_ReturnType Rte_Read_DoorStatus(Rte_DoorStatusType *data)
{
    if (data == NULL)
    {
        return LS_E_NOT_OK;
    }
    *data = s_doorStatus;
    return LS_E_OK;
}

void Rte_Write_WinStatus(const Rte_WinStatusType *data)
{
    if (data != NULL)
    {
        Com_WinStsType sts;

        s_winStatus = *data;
        sts.state = data->state;
        sts.stopReason = data->stopReason;
        sts.pressIdEcho = data->pressIdEcho;
        sts.result = data->result;
        sts.currentRaw = 0xFFu;
        Com_SetWinSts(&sts);
    }
}

void Rte_Write_DoorStatus(const Rte_DoorStatusType *data)
{
    if (data != NULL)
    {
        Com_DoorStsType sts;

        s_doorStatus = *data;
        sts.state = data->state;
        sts.lastReqId = data->lastReqId;
        sts.result = data->lastResult;
        sts.rateLimited = data->rateLimited;
        Com_SetDoorSts(&sts);
    }
}

Ls_ReturnType Rte_Call_WinBridge_Set(Rte_BridgeCmdType cmd, uint16_t dutyPermille)
{
    return HBridge_Set(HBRIDGE_CH_WIN, cmd, dutyPermille);
}

void Rte_Call_LockAct_Stop(void)
{
    LockAct_Stop();
}

void Rte_Call_EnterSafe(Rte_SafeCauseType cause)
{
    SafeMon_EnterSafe(cause);
}

uint32_t Rte_Call_TimeMs(void)
{
    return TBase_Ms();
}
