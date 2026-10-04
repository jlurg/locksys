/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file ls_e2e.h
 * @brief LockSys CAN E2E profile: CRC-8/SAE-J1850 with DataID and 4-bit alive counter.
 *
 * Profile (LS-SAIC-001 section 7.2):
 * - byte 0 = CRC; bits 0-3 of byte 1 = alive counter (0 .. 15, wrapping);
 * - CRC input = DataID low byte, DataID high byte, frame bytes 1 .. DLC-1; the DataID is not
 *   transmitted;
 * - receiver (cyclic mode): a DLC or CRC mismatch is a CRC error; the first frame after
 *   start-up or after an RX timeout is OK and sets the reference counter; delta =
 *   (counter - reference) mod 16: 0 is REPEATED, 1 .. MaxDelta is OK, above MaxDelta is
 *   WRONG_SEQUENCE and resynchronises the reference; nOkValid consecutive OK frames give
 *   VALID, nErrInvalid consecutive errors or an RX timeout give INVALID;
 * - receiver (event mode): DLC and CRC are checked, the counter is not sequence-checked;
 * - the data of a frame may be used only when the frame is OK and the state after it is
 *   VALID (LsE2e_IsDataValid()).
 *
 * E2E is a safety mechanism against corruption, repetition, loss, delay and masquerading
 * inside the system; it is not a security control.
 */

#ifndef LS_E2E_H
#define LS_E2E_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Smallest frame length that carries the CRC and the alive counter. */
#define LS_E2E_MIN_DLC (2u)
/** @brief Largest classical CAN frame length. */
#define LS_E2E_MAX_DLC (8u)
/** @brief Alive counter mask (4 bits). */
#define LS_E2E_COUNTER_MASK (0x0Fu)
/** @brief Largest MaxDelta that still detects a repeated frame. */
#define LS_E2E_MAX_DELTA_MAX (14u)

    /** @brief Receiver mode of a message (DBC attribute LsE2eMode). */
    typedef uint8_t LsE2e_ModeType;
/** @brief Message without E2E protection (not accepted by this module). */
#define LS_E2E_MODE_NONE ((LsE2e_ModeType)0u)
/** @brief Cyclic message: counter sequence checked, RX timeout supervised by the caller. */
#define LS_E2E_MODE_CYCLIC ((LsE2e_ModeType)1u)
/** @brief Event message: CRC and DLC checked, counter not sequence-checked. */
#define LS_E2E_MODE_EVENT ((LsE2e_ModeType)2u)

    /** @brief Result of a frame check. */
    typedef uint8_t LsE2e_CheckStatusType;
/** @brief Frame accepted. */
#define LS_E2E_STATUS_OK ((LsE2e_CheckStatusType)0u)
/** @brief DLC or CRC mismatch (also a frame received under a wrong identifier). */
#define LS_E2E_STATUS_CRC_ERROR ((LsE2e_CheckStatusType)1u)
/** @brief Alive counter equal to the reference counter. */
#define LS_E2E_STATUS_REPEATED ((LsE2e_CheckStatusType)2u)
/** @brief Alive counter more than MaxDelta ahead of the reference counter. */
#define LS_E2E_STATUS_WRONG_SEQUENCE ((LsE2e_CheckStatusType)3u)
/** @brief Invalid argument or configuration; the receiver state is unchanged. */
#define LS_E2E_STATUS_BAD_ARG ((LsE2e_CheckStatusType)4u)

    /** @brief Receiver state of a message. */
    typedef uint8_t LsE2e_StateType;
/** @brief No frame accepted since initialisation; data not usable. */
#define LS_E2E_STATE_INIT ((LsE2e_StateType)0u)
/** @brief Data usable. */
#define LS_E2E_STATE_VALID ((LsE2e_StateType)1u)
/** @brief Consecutive errors or RX timeout; data not usable. */
#define LS_E2E_STATE_INVALID ((LsE2e_StateType)2u)

    /** @brief Result of the protect and initialisation functions. */
    typedef uint8_t LsE2e_ResultType;
/** @brief Operation done. */
#define LS_E2E_E_OK ((LsE2e_ResultType)0u)
/** @brief Invalid argument or configuration; nothing changed. */
#define LS_E2E_E_BAD_ARG ((LsE2e_ResultType)1u)

    /** @brief E2E configuration of one message (from the DBC attributes and the parameters). */
    typedef struct
    {
        uint16_t dataId;     /**< LsE2eDataId */
        uint8_t dlc;         /**< fixed frame length, LS_E2E_MIN_DLC .. LS_E2E_MAX_DLC */
        LsE2e_ModeType mode; /**< LS_E2E_MODE_CYCLIC or LS_E2E_MODE_EVENT */
        uint8_t maxDelta;    /**< LsE2eMaxDelta, 1 .. LS_E2E_MAX_DELTA_MAX (cyclic mode) */
        uint8_t nOkValid;    /**< consecutive OK frames for VALID (n_e2e_ok_valid), >= 1 */
        uint8_t nErrInvalid; /**< consecutive errors for INVALID (n_e2e_err_invalid), >= 1 */
    } LsE2e_ConfigType;

    /** @brief Sender state of one message. */
    typedef struct
    {
        uint8_t counter; /**< alive counter of the next protected frame */
    } LsE2e_TxStateType;

    /** @brief Receiver state of one message. Initialise with LsE2e_RxInit(). */
    typedef struct
    {
        LsE2e_StateType state;            /**< receiver state */
        LsE2e_CheckStatusType lastStatus; /**< status of the last checked frame */
        uint8_t refCounter;               /**< reference alive counter */
        bool refValid;                    /**< reference counter set */
        uint8_t okCount;                  /**< consecutive OK frames (saturating) */
        uint8_t errCount;                 /**< consecutive errors (saturating) */
        uint16_t crcErrors;               /**< CRC and DLC errors (saturating) */
        uint16_t seqErrors;               /**< WRONG_SEQUENCE frames (saturating) */
        uint16_t repeated;                /**< REPEATED frames (saturating) */
        uint16_t timeouts;                /**< RX timeouts (saturating) */
    } LsE2e_RxStateType;

    /**
 * @brief Check an E2E configuration.
 *
 * @param cfg Configuration.
 * @return true when @p cfg is non-NULL and every field is in range.
 */
    bool LsE2e_IsConfigValid(const LsE2e_ConfigType *cfg);

    /**
 * @brief Compute the E2E CRC of a frame (byte 0 is excluded from the computation).
 *
 * @param dataId DataID of the message.
 * @param frame  Frame bytes.
 * @param len    Frame length (1 .. LS_E2E_MAX_DLC).
 * @return CRC-8/SAE-J1850 over DataID low, DataID high, frame[1] .. frame[len-1]; the CRC of
 *         the DataID alone when @p frame is NULL or @p len is 0.
 */
    uint8_t LsE2e_Crc(uint16_t dataId, const uint8_t *frame, uint8_t len);

    /**
 * @brief Initialise a sender state (first alive counter 0).
 *
 * @param tx Sender state.
 * @return LS_E2E_E_OK, or LS_E2E_E_BAD_ARG for a NULL pointer.
 */
    LsE2e_ResultType LsE2e_TxInit(LsE2e_TxStateType *tx);

    /**
 * @brief Write a given alive counter and the CRC into a frame.
 *
 * Bits 4-7 of byte 1 (signal data) are kept.
 *
 * @param cfg     Message configuration.
 * @param counter Alive counter (only bits 0-3 are used).
 * @param frame   Frame of cfg->dlc bytes; bytes 1 .. DLC-1 hold the payload.
 * @param len     Frame length; must equal cfg->dlc.
 * @return LS_E2E_E_OK, or LS_E2E_E_BAD_ARG (NULL pointer, invalid configuration, wrong length).
 */
    LsE2e_ResultType LsE2e_ProtectWithCounter(const LsE2e_ConfigType *cfg, uint8_t counter,
                                              uint8_t *frame, uint8_t len);

    /**
 * @brief Protect a frame with the next alive counter of the sender and advance the counter.
 *
 * Call once for every frame handed to the CAN controller or driver; a caller that cannot
 * hand the frame over restores the previous sender state.
 *
 * @param cfg   Message configuration.
 * @param tx    Sender state.
 * @param frame Frame of cfg->dlc bytes.
 * @param len   Frame length; must equal cfg->dlc.
 * @return LS_E2E_E_OK, or LS_E2E_E_BAD_ARG (nothing changed).
 */
    LsE2e_ResultType LsE2e_Protect(const LsE2e_ConfigType *cfg, LsE2e_TxStateType *tx,
                                   uint8_t *frame, uint8_t len);

    /**
 * @brief Initialise a receiver state: INIT, no reference counter, counters zero.
 *
 * @param rx Receiver state.
 * @return LS_E2E_E_OK, or LS_E2E_E_BAD_ARG for a NULL pointer.
 */
    LsE2e_ResultType LsE2e_RxInit(LsE2e_RxStateType *rx);

    /**
 * @brief Check one received frame and update the receiver state.
 *
 * Every received frame of the message is checked in arrival order.
 *
 * @param cfg   Message configuration.
 * @param rx    Receiver state.
 * @param frame Received frame bytes.
 * @param len   Received length (DLC).
 * @return Frame status; LS_E2E_STATUS_BAD_ARG leaves @p rx unchanged.
 */
    LsE2e_CheckStatusType LsE2e_Check(const LsE2e_ConfigType *cfg, LsE2e_RxStateType *rx,
                                      const uint8_t *frame, uint8_t len);

    /**
 * @brief Report an RX timeout of a cyclic message: INVALID, reference counter cleared.
 *
 * @param rx Receiver state; NULL is ignored.
 */
    void LsE2e_RxTimeout(LsE2e_RxStateType *rx);

    /**
 * @brief Test whether the data of the last checked frame may be used.
 *
 * @param rx Receiver state.
 * @return true when the last frame was OK and the state after it is VALID.
 */
    bool LsE2e_IsDataValid(const LsE2e_RxStateType *rx);

#ifdef __cplusplus
}
#endif

#endif /* LS_E2E_H */
