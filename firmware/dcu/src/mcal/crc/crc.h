/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file crc.h
 * @brief CRC-32 unit: polynomial 0x04C11DB7, initial value 0xFFFFFFFF, 32-bit words.
 *
 * The unit computes CRC-32/MPEG-2 over little-endian words fed most significant byte first,
 * which matches the `ielftool --checksum crc32:Li,0xFFFFFFFF` post-link step and
 * tools/romcrc/verify_rom_crc.py. Owned by SafeMon.
 */

#ifndef CRC_H
#define CRC_H

#include "platform/ls_std_types.h"

/**
 * @brief Enables the CRC unit clock and resets the data register to 0xFFFFFFFF.
 */
void Crc_Reset(void);

/**
 * @brief Feeds words into the CRC unit.
 *
 * @param[in] words Words to feed.
 * @param[in] count Number of words.
 */
void Crc_Feed(const uint32_t *words, uint32_t count);

/**
 * @brief Returns the CRC of the words fed since the last reset.
 *
 * @return CRC-32 value.
 */
uint32_t Crc_Value(void);

#endif /* CRC_H */
