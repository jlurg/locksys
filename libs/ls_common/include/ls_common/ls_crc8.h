/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file ls_crc8.h
 * @brief CRC-8/SAE-J1850 (polynomial 0x1D, initial value 0xFF, final XOR 0xFF, no reflection).
 *
 * Check value of the ASCII string "123456789": 0x4B (LS-SAIC-001 section 7.2).
 */

#ifndef LS_CRC8_H
#define LS_CRC8_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Initial value of a CRC-8/SAE-J1850 computation. */
#define LS_CRC8_J1850_INIT (0xFFu)
/** @brief Final XOR value of a CRC-8/SAE-J1850 computation. */
#define LS_CRC8_J1850_XOROUT (0xFFu)

    /**
 * @brief Continue a CRC-8/SAE-J1850 computation over a block of bytes.
 *
 * Neither the initial value nor the final XOR is applied, so a CRC over non-contiguous data
 * is computed as LsCrc8_J1850Update() calls chained from LS_CRC8_J1850_INIT, followed by
 * an XOR with LS_CRC8_J1850_XOROUT.
 *
 * @param crc  Intermediate CRC value (LS_CRC8_J1850_INIT for the first block).
 * @param data Data bytes; may be NULL only when @p len is 0.
 * @param len  Number of bytes.
 * @return Intermediate CRC value after @p data; @p crc unchanged when @p data is NULL.
 */
    uint8_t LsCrc8_J1850Update(uint8_t crc, const uint8_t *data, uint32_t len);

    /**
 * @brief Compute the complete CRC-8/SAE-J1850 of a block of bytes.
 *
 * @param data Data bytes; may be NULL only when @p len is 0.
 * @param len  Number of bytes.
 * @return CRC value (initial value and final XOR applied).
 */
    uint8_t LsCrc8_J1850(const uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* LS_CRC8_H */
