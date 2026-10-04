/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file ls_compiler.h
 * @brief Compiler abstraction: the only header that uses language extensions (permit DP-02).
 *
 * Branches: IAR EWARM (__ICCARM__), GCC for Arm targets (__GNUC__ and __arm__) and the host
 * build of the unit tests (any other compiler). On the host the core registers used by the
 * platform layer are plain variables provided by the test support code.
 */

#ifndef LS_COMPILER_H
#define LS_COMPILER_H

#include <stdint.h>

/** @brief Expands to a pragma whose text is the stringised argument. */
#define LS_PRAGMA(x) _Pragma(#x)

#if defined(__ICCARM__)

#include <intrinsics.h>

/** @brief Object excluded from start-up initialisation (placed in the noinit region). */
#define LS_NOINIT __no_init
/** @brief Places the next declared object in section @p s (string literal). */
#define LS_SECTION(s) LS_PRAGMA(location = s)
/** @brief Marks the next function as an interrupt call-graph root for stack analysis. */
#define LS_ISR_ROOT LS_PRAGMA(call_graph_root = "interrupt")
/** @brief Keeps the next object or function in the image although it is not referenced. */
#define LS_KEEP __root

#define LS_DSB()          __DSB()                     /**< Data synchronisation barrier */
#define LS_ISB()          __ISB()                     /**< Instruction synchronisation barrier */
#define LS_DMB()          __DMB()                     /**< Data memory barrier */
#define LS_GET_BASEPRI()  ((uint32_t)__get_BASEPRI()) /**< Reads BASEPRI */
#define LS_SET_BASEPRI(v) __set_BASEPRI((v))          /**< Writes BASEPRI */
#define LS_DISABLE_IRQ()  __disable_interrupt()       /**< Sets PRIMASK */
#define LS_ENABLE_IRQ()   __enable_interrupt()        /**< Clears PRIMASK */
#define LS_NOP()          __no_operation()            /**< No operation */

#elif defined(__GNUC__) && defined(__arm__)

#define LS_NOINIT     __attribute__((section(".noinit")))
#define LS_SECTION(s) __attribute__((section(s)))
#define LS_ISR_ROOT   __attribute__((used))
#define LS_KEEP       __attribute__((used))

static inline void Ls_Dsb(void)
{
    __asm volatile("dsb 0xF" ::: "memory");
}

static inline void Ls_Isb(void)
{
    __asm volatile("isb 0xF" ::: "memory");
}

static inline void Ls_Dmb(void)
{
    __asm volatile("dmb 0xF" ::: "memory");
}

static inline uint32_t Ls_GetBasepri(void)
{
    uint32_t value;
    __asm volatile("mrs %0, basepri" : "=r"(value));
    return value;
}

static inline void Ls_SetBasepri(uint32_t value)
{
    __asm volatile("msr basepri, %0" : : "r"(value) : "memory");
}

#define LS_DSB()          Ls_Dsb()
#define LS_ISB()          Ls_Isb()
#define LS_DMB()          Ls_Dmb()
#define LS_GET_BASEPRI()  Ls_GetBasepri()
#define LS_SET_BASEPRI(v) Ls_SetBasepri((v))
#define LS_DISABLE_IRQ()  __asm volatile("cpsid i" ::: "memory")
#define LS_ENABLE_IRQ()   __asm volatile("cpsie i" ::: "memory")
#define LS_NOP()          __asm volatile("nop")

#else /* host build of the unit tests */

/** @brief Host model of BASEPRI; defined by the test support code. */
extern uint32_t LsHost_Basepri;
/** @brief Host model of PRIMASK; defined by the test support code. */
extern uint32_t LsHost_Primask;

#define LS_NOINIT
#define LS_SECTION(s)
#define LS_ISR_ROOT
#define LS_KEEP
#define LS_DSB()
#define LS_ISB()
#define LS_DMB()
#define LS_GET_BASEPRI()  (LsHost_Basepri)
#define LS_SET_BASEPRI(v) (LsHost_Basepri = (v))
#define LS_DISABLE_IRQ()  (LsHost_Primask = 1u)
#define LS_ENABLE_IRQ()   (LsHost_Primask = 0u)
#define LS_NOP()

#endif

#endif /* LS_COMPILER_H */
