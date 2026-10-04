/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file ls_ring.h
 * @brief Single-producer single-consumer byte ring buffer.
 *
 * The producer writes whole records (LsRing_Push() is all or nothing, so a record is never
 * split by an overflow); the consumer reads with LsRing_Pop() or, for DMA transmission,
 * with LsRing_PeekContiguous() followed by LsRing_Consume(). The capacity is a power of two
 * and the free-running 32-bit indices are written by one side each, so one producer context
 * and one consumer context (for example a task and an interrupt) need no lock when 32-bit
 * stores are atomic. LS_RING_BARRIER() orders the data accesses against the index updates;
 * it may be defined by the build (for example as a data memory barrier on a multi-core target).
 */

#ifndef LS_RING_H
#define LS_RING_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef LS_RING_BARRIER
/** @brief Memory barrier between data and index accesses; empty for single-core targets. */
#define LS_RING_BARRIER()
#endif

    /** @brief Result of the ring operations. */
    typedef uint8_t LsRing_ResultType;
/** @brief Operation done. */
#define LS_RING_OK ((LsRing_ResultType)0u)
/** @brief Not enough free space; nothing was written. */
#define LS_RING_E_FULL ((LsRing_ResultType)1u)
/** @brief Invalid argument; nothing changed. */
#define LS_RING_E_PARAM ((LsRing_ResultType)2u)

    /** @brief Ring buffer. Initialise with LsRing_Init(); do not access the fields directly. */
    typedef struct
    {
        uint8_t *buf;           /**< storage of capacity bytes */
        uint32_t mask;          /**< capacity - 1 */
        volatile uint32_t head; /**< bytes written since initialisation (producer only) */
        volatile uint32_t tail; /**< bytes read since initialisation (consumer only) */
    } LsRing_Type;

    /**
 * @brief Initialise an empty ring over caller-provided storage.
 *
 * @param ring     Ring buffer.
 * @param buf      Storage of @p capacity bytes, owned by the caller for the ring lifetime.
 * @param capacity Capacity in bytes: a power of two from 2 to 2^31.
 * @return LS_RING_OK, or LS_RING_E_PARAM for NULL arguments or an invalid capacity.
 */
    LsRing_ResultType LsRing_Init(LsRing_Type *ring, uint8_t *buf, uint32_t capacity);

    /**
 * @brief Number of bytes available to the consumer.
 *
 * @param ring Ring buffer.
 * @return Used bytes; 0 for a NULL or uninitialised ring.
 */
    uint32_t LsRing_Used(const LsRing_Type *ring);

    /**
 * @brief Number of bytes available to the producer.
 *
 * @param ring Ring buffer.
 * @return Free bytes; 0 for a NULL or uninitialised ring.
 */
    uint32_t LsRing_Free(const LsRing_Type *ring);

    /**
 * @brief Append a record; the record is written completely or not at all (producer side).
 *
 * @param ring Ring buffer.
 * @param data Record bytes.
 * @param len  Record length in bytes (0 is accepted and writes nothing).
 * @return LS_RING_OK, LS_RING_E_FULL when fewer than @p len bytes are free, or
 *         LS_RING_E_PARAM for NULL arguments or an uninitialised ring.
 */
    LsRing_ResultType LsRing_Push(LsRing_Type *ring, const uint8_t *data, uint32_t len);

    /**
 * @brief Copy up to @p maxLen bytes out of the ring and release them (consumer side).
 *
 * @param ring   Ring buffer.
 * @param dst    Destination buffer.
 * @param maxLen Capacity of @p dst in bytes.
 * @return Number of bytes copied; 0 for NULL arguments.
 */
    uint32_t LsRing_Pop(LsRing_Type *ring, uint8_t *dst, uint32_t maxLen);

    /**
 * @brief Locate the largest block of used bytes that is contiguous in memory (consumer side).
 *
 * @param ring Ring buffer.
 * @param data Receives the address of the first used byte (NULL when nothing is used).
 * @return Length of the contiguous block; 0 for NULL arguments or an empty ring.
 */
    uint32_t LsRing_PeekContiguous(const LsRing_Type *ring, const uint8_t **data);

    /**
 * @brief Release bytes read through LsRing_PeekContiguous() (consumer side).
 *
 * @param ring Ring buffer.
 * @param len  Number of bytes to release; limited to the used bytes.
 * @return Number of bytes released.
 */
    uint32_t LsRing_Consume(LsRing_Type *ring, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* LS_RING_H */
