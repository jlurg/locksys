/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file com.c
 * @brief CAN communication: generated pack and unpack, E2E, RX timeouts and the TX schedule.
 *
 * Skeleton: the behaviour is delivered at milestone M1; until then every
 * function returns its safe value.
 */

#include "services/com/com.h"

void Com_Init(void)
{
}

void Com_MainRx(void)
{
}

void Com_MainTx(void)
{
}

Ls_ReturnType Com_GetWinCmd(Com_WinCmdType *cmd)
{
    (void)cmd;
    return LS_E_NOT_OK;
}

Ls_ReturnType Com_GetDoorCmd(Com_DoorCmdType *cmd)
{
    (void)cmd;
    return LS_E_NOT_OK;
}

Ls_ReturnType Com_GetCgwNodeSts(Com_CgwNodeStsType *sts)
{
    (void)sts;
    return LS_E_NOT_OK;
}

void Com_SetWinSts(const Com_WinStsType *sts)
{
    (void)sts;
}

void Com_SetWinMotion(const Com_WinMotionType *motion)
{
    (void)motion;
}

void Com_SetDoorSts(const Com_DoorStsType *sts)
{
    (void)sts;
}

void Com_SetTempSts(const Com_TempStsType *sts)
{
    (void)sts;
}

void Com_SetNodeSts(const Com_NodeStsType *sts)
{
    (void)sts;
}

void Com_TriggerTx(Com_TxFrameType frame)
{
    (void)frame;
}
