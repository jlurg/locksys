/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file ls_ring.c
 * @brief Single-producer single-consumer byte ring buffer.
 */

#include "ls_common/ls_ring.h"

#include <stdbool.h>
#include <stddef.h>

#define LS_RING_MAX_CAPACITY (0x80000000u)

static bool LsRing_IsReady(const LsRing_Type *ring)
{
    return (ring != NULL) && (ring->buf != NULL);
}

LsRing_ResultType LsRing_Init(LsRing_Type *ring, uint8_t *buf, uint32_t capacity)
{
    LsRing_ResultType result = LS_RING_E_PARAM;

    if (ring != NULL)
    {
        ring->buf = NULL;
        ring->mask = 0u;
        ring->head = 0u;
        ring->tail = 0u;
        if ((buf != NULL) && (capacity >= 2u) && (capacity <= LS_RING_MAX_CAPACITY) &&
            ((capacity & (capacity - 1u)) == 0u))
        {
            ring->buf = buf;
            ring->mask = capacity - 1u;
            result = LS_RING_OK;
        }
    }
    return result;
}

uint32_t LsRing_Used(const LsRing_Type *ring)
{
    uint32_t used = 0u;

    if (LsRing_IsReady(ring))
    {
        used = ring->head - ring->tail;
    }
    return used;
}

uint32_t LsRing_Free(const LsRing_Type *ring)
{
    uint32_t freeBytes = 0u;

    if (LsRing_IsReady(ring))
    {
        freeBytes = (ring->mask + 1u) - (ring->head - ring->tail);
    }
    return freeBytes;
}

LsRing_ResultType LsRing_Push(LsRing_Type *ring, const uint8_t *data, uint32_t len)
{
    LsRing_ResultType result = LS_RING_E_PARAM;

    if (LsRing_IsReady(ring) && (data != NULL))
    {
        if (len > LsRing_Free(ring))
        {
            result = LS_RING_E_FULL;
        }
        else
        {
            uint32_t head = ring->head;

            for (uint32_t i = 0u; i < len; i++)
            {
                ring->buf[(head + i) & ring->mask] = data[i];
            }
            LS_RING_BARRIER();
            ring->head = head + len;
            result = LS_RING_OK;
        }
    }
    return result;
}

uint32_t LsRing_Pop(LsRing_Type *ring, uint8_t *dst, uint32_t maxLen)
{
    uint32_t count = 0u;

    if (LsRing_IsReady(ring) && (dst != NULL))
    {
        uint32_t tail = ring->tail;
        uint32_t used = ring->head - tail;

        count = (used < maxLen) ? used : maxLen;
        LS_RING_BARRIER();
        for (uint32_t i = 0u; i < count; i++)
        {
            dst[i] = ring->buf[(tail + i) & ring->mask];
        }
        LS_RING_BARRIER();
        ring->tail = tail + count;
    }
    return count;
}

uint32_t LsRing_PeekContiguous(const LsRing_Type *ring, const uint8_t **data)
{
    uint32_t length = 0u;

    if (data != NULL)
    {
        *data = NULL;
        if (LsRing_IsReady(ring))
        {
            uint32_t tail = ring->tail;
            uint32_t used = ring->head - tail;
            uint32_t offset = tail & ring->mask;
            uint32_t toEnd = (ring->mask + 1u) - offset;

            length = (used < toEnd) ? used : toEnd;
            if (length > 0u)
            {
                LS_RING_BARRIER();
                *data = &ring->buf[offset];
            }
        }
    }
    return length;
}

uint32_t LsRing_Consume(LsRing_Type *ring, uint32_t len)
{
    uint32_t count = 0u;

    if (LsRing_IsReady(ring))
    {
        uint32_t tail = ring->tail;
        uint32_t used = ring->head - tail;

        count = (len < used) ? len : used;
        LS_RING_BARRIER();
        ring->tail = tail + count;
    }
    return count;
}
