/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file com.h
 * @brief CAN communication: generated pack and unpack, E2E, RX timeouts and the TX schedule.
 */

#ifndef COM_H
#define COM_H

#include "platform/ls_std_types.h"
#include "ls_enums_gen.h"

/** @brief Received CGW_WinCmd content and E2E state. */
typedef struct
{
    Ls_WindowRequestType req; /**< WindowRequest */
    uint8_t pressId;          /**< PressId */
    uint8_t holdAgeRaw;       /**< HoldAge, raw */
    uint8_t e2eState;         /**< E2E receiver state (ls_e2e) */
    bool timeout;             /**< RX timeout active */
} Com_WinCmdType;

/** @brief Received CGW_DoorCmd content and E2E state. */
typedef struct
{
    Ls_DoorRequestType req; /**< DoorRequest */
    uint8_t reqId;          /**< ReqId */
    uint8_t e2eState;       /**< E2E receiver state */
    bool timeout;           /**< RX timeout active */
} Com_DoorCmdType;

/** @brief Received CGW_NodeSts content. */
typedef struct
{
    Ls_NodeModeType mode; /**< CGW NodeMode */
    uint8_t matrixMajor;  /**< CAN matrix major version */
    uint8_t matrixMinor;  /**< CAN matrix minor version */
    bool timeout;         /**< RX timeout active */
} Com_CgwNodeStsType;

/** @brief Content of DCU_WinSts. */
typedef struct
{
    Ls_WindowStateType state;           /**< WindowState */
    Ls_WindowStopReasonType stopReason; /**< StopReason */
    uint8_t pressIdEcho;                /**< PressIdEcho */
    Ls_CommandResultType result;        /**< WinResult */
    uint8_t currentRaw;                 /**< Current in 0.1 A, 255 = invalid */
} Com_WinStsType;

/** @brief Content of DCU_WinMotion (0x201, 50 ms). */
typedef struct
{
    Ls_EncoderStatusType encoderStatus; /**< EncoderStatus */
    int16_t speedRpmX10;                /**< Output-shaft speed in 0.1 rpm */
    uint8_t dutyPct;                    /**< Commanded duty */
    int32_t position;                   /**< Relative position in counts */
} Com_WinMotionType;

/** @brief Content of DCU_DoorSts. */
typedef struct
{
    Ls_DoorLockStateType state;  /**< DoorLockState */
    uint8_t lastReqId;           /**< LastReqId */
    Ls_CommandResultType result; /**< LastResult */
    bool rateLimited;            /**< RateLimited */
} Com_DoorStsType;

/** @brief Content of DCU_TempSts. */
typedef struct
{
    int16_t cdeg;             /**< Temperature in 0.01 degC */
    Ls_TempStatusType status; /**< TempStatus */
    uint8_t sampleSeq;        /**< SampleSeq */
} Com_TempStsType;

/** @brief Content of DCU_NodeSts. */
typedef struct
{
    Ls_NodeModeType mode;           /**< NodeMode */
    Ls_ResetReasonType resetReason; /**< ResetReason */
    uint8_t dtcCount;               /**< Confirmed DTC count */
    uint8_t cpuLoadMaxPct;          /**< Maximum CPU load */
} Com_NodeStsType;

/** @brief Transmitted frame. */
typedef uint8_t Com_TxFrameType;

#define COM_TX_WIN_STS    ((Com_TxFrameType)0u) /**< DCU_WinSts */
#define COM_TX_WIN_MOTION ((Com_TxFrameType)1u) /**< DCU_WinMotion */
#define COM_TX_DOOR_STS   ((Com_TxFrameType)2u) /**< DCU_DoorSts */
#define COM_TX_TEMP_STS   ((Com_TxFrameType)3u) /**< DCU_TempSts */
#define COM_TX_NODE_STS   ((Com_TxFrameType)4u) /**< DCU_NodeSts */
#define COM_TX_VERSION    ((Com_TxFrameType)5u) /**< DCU_Version */

/**
 * @brief Loads the start values of every transmitted frame.
 */
void Com_Init(void);

/**
 * @brief Unpacks and E2E-checks received frames in arrival order; supervises RX timeouts.
 *
 * @note 10 ms task.
 */
void Com_MainRx(void);

/**
 * @brief Packs and E2E-protects due frames and hands them to CanIf.
 *
 * @note 10 ms task.
 */
void Com_MainTx(void);

/**
 * @brief Returns the latest CGW_WinCmd.
 *
 * @param[out] cmd Latest CGW_WinCmd.
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType Com_GetWinCmd(Com_WinCmdType *cmd);

/**
 * @brief Returns the latest CGW_DoorCmd.
 *
 * @param[out] cmd Latest CGW_DoorCmd.
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType Com_GetDoorCmd(Com_DoorCmdType *cmd);

/**
 * @brief Returns the latest CGW_NodeSts.
 *
 * @param[out] sts Latest CGW_NodeSts.
 * @retval LS_E_OK     Completed.
 * @retval LS_E_NOT_OK Not completed or not available yet.
 */
Ls_ReturnType Com_GetCgwNodeSts(Com_CgwNodeStsType *sts);

/**
 * @brief Updates DCU_WinSts.
 *
 * @param[in] sts New content.
 */
void Com_SetWinSts(const Com_WinStsType *sts);

/**
 * @brief Updates DCU_WinMotion.
 *
 * @param[in] motion New content.
 */
void Com_SetWinMotion(const Com_WinMotionType *motion);

/**
 * @brief Updates DCU_DoorSts.
 *
 * @param[in] sts New content.
 */
void Com_SetDoorSts(const Com_DoorStsType *sts);

/**
 * @brief Updates DCU_TempSts.
 *
 * @param[in] sts New content.
 */
void Com_SetTempSts(const Com_TempStsType *sts);

/**
 * @brief Updates DCU_NodeSts.
 *
 * @param[in] sts New content.
 */
void Com_SetNodeSts(const Com_NodeStsType *sts);

/**
 * @brief Requests an event transmission, respecting the minimum gap.
 *
 * @param[in] frame Frame to send.
 */
void Com_TriggerTx(Com_TxFrameType frame);

#endif /* COM_H */
