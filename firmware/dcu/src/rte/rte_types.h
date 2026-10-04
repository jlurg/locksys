/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file rte_types.h
 * @brief Data types of the RTE ports (LS-DCU-SAD-001 section 7.1).
 */

#ifndef RTE_TYPES_H
#define RTE_TYPES_H

#include "ls_enums_gen.h"
#include "platform/ls_std_types.h"

/** @brief State of the received command frame. */
typedef uint8_t Rte_FrameStateType;

#define RTE_FRAME_INVALID ((Rte_FrameStateType)0u) /**< E2E INVALID or not yet VALID */
#define RTE_FRAME_VALID   ((Rte_FrameStateType)1u) /**< E2E VALID */
#define RTE_FRAME_TIMEOUT ((Rte_FrameStateType)2u) /**< RX timeout */

/** @brief Port WinRequest (writer CmdArb). */
typedef struct
{
    Ls_WindowRequestType req; /**< WindowRequest */
    uint8_t pressId;          /**< PressId */
    uint16_t holdAgeMs;       /**< HoldAge in ms */
    Rte_FrameStateType frame; /**< Frame state */
    bool rearmed;             /**< Re-armed by a VALID frame with Req = STOP */
    uint8_t firstPressId;     /**< First PressId received with E2E OK after reset */
    bool firstPressIdValid;   /**< firstPressId is available */
} Rte_WinRequestType;

/** @brief Port DoorRequest (writer CmdArb). */
typedef struct
{
    Ls_DoorRequestType req; /**< DoorRequest */
    uint8_t reqId;          /**< ReqId */
    bool newRequest;        /**< ReqId differs from LastReqId */
} Rte_DoorRequestType;

/** @brief Port CgwStatus (writer CmdArb). */
typedef struct
{
    Ls_NodeModeType mode; /**< CGW NodeMode */
    uint8_t matrixMajor;  /**< CAN matrix major version */
    uint8_t matrixMinor;  /**< CAN matrix minor version */
    bool versionKnown;    /**< Version received */
    bool versionMatch;    /**< Version equals the DCU matrix version */
    bool alive;           /**< CGW_NodeSts received within its timeout */
} Rte_CgwStatusType;

/** @brief Port EcuMode (writer ModeMgr). */
typedef struct
{
    Ls_NodeModeType mode; /**< DCU NodeMode */
    bool inhibitUp;       /**< Window UP inhibited */
    bool inhibitDown;     /**< Window DOWN inhibited */
    bool inhibitLock;     /**< Lock actuation inhibited */
    bool safeLatched;     /**< SAFE latch set */
} Rte_EcuModeType;

/** @brief Port WinStatus (writer WinCtrl). */
typedef struct
{
    Ls_WindowStateType state;           /**< WindowState */
    Ls_WindowStopReasonType stopReason; /**< StopReason of the last stop or refusal */
    uint8_t pressIdEcho;                /**< PressIdEcho */
    Ls_CommandResultType result;        /**< WinResult */
    uint8_t faults;                     /**< Window fault flags */
} Rte_WinStatusType;

/** @brief Port DoorStatus (writer DoorCtrl). */
typedef struct
{
    Ls_DoorLockStateType state;      /**< DoorLockState */
    uint8_t lastReqId;               /**< LastReqId */
    Ls_CommandResultType lastResult; /**< LastResult */
    bool rateLimited;                /**< RateLimited */
} Rte_DoorStatusType;

/** @brief Bridge command of the call ports (values equal HBRIDGE_CMD_*). */
typedef uint8_t Rte_BridgeCmdType;

#define RTE_BRIDGE_OFF        ((Rte_BridgeCmdType)0u) /**< Coast */
#define RTE_BRIDGE_DRIVE_UP   ((Rte_BridgeCmdType)1u) /**< Drive UP */
#define RTE_BRIDGE_DRIVE_DOWN ((Rte_BridgeCmdType)2u) /**< Drive DOWN */
#define RTE_BRIDGE_BRAKE      ((Rte_BridgeCmdType)3u) /**< Brake */

/** @brief Cause of a SAFE request (values equal SAFEMON_CAUSE_*). */
typedef uint8_t Rte_SafeCauseType;

#define RTE_SAFE_CAUSE_MODE   ((Rte_SafeCauseType)1u) /**< Critical cause from ModeMgr */
#define RTE_SAFE_CAUSE_ENGINE ((Rte_SafeCauseType)2u) /**< State-machine engine fault */

#endif /* RTE_TYPES_H */
