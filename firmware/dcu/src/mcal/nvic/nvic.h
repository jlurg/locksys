/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file nvic.h
 * @brief Interrupt priorities, fault enables and fault information (LS-DCU-SAD-001 section 5.2).
 */

#ifndef NVIC_H
#define NVIC_H

#include "platform/ls_std_types.h"

/** @brief Fault status snapshot taken in a fault handler. */
typedef struct
{
    uint32_t cfsr; /**< SCB_CFSR */
    uint32_t hfsr; /**< SCB_HFSR */
    uint32_t bfar; /**< SCB_BFAR */
} Nvic_FaultInfoType;

/**
 * @brief Sets 4 preemption bits, the priority of every used exception and IRQ, and enables
 *        the BusFault and UsageFault handlers. IRQs stay disabled until their driver enables them.
 */
void Nvic_Init(void);

/**
 * @brief Captures the fault status registers.
 *
 * @param[out] info Snapshot; not written when NULL.
 */
void Nvic_GetFaultInfo(Nvic_FaultInfoType *info);

/**
 * @brief Reports whether a debugger is attached (DHCSR.C_DEBUGEN).
 *
 * @return true when a debugger is attached.
 */
bool Nvic_IsDebuggerAttached(void);

/**
 * @brief Requests a system reset (SCB_AIRCR.SYSRESETREQ).
 *
 * @note Does not return on the target.
 */
void Nvic_SystemReset(void);

#endif /* NVIC_H */
