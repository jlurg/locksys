/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file rte.h
 * @brief RTE initialisation; port access is declared in the per-SWC views rte_<swc>.h.
 */

#ifndef RTE_H
#define RTE_H

/**
 * @brief Loads the initial value of every SWC-written port (safe values: enumeration 0).
 */
void Rte_Init(void);

#endif /* RTE_H */
