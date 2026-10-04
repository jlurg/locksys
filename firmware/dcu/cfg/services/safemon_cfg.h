/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file safemon_cfg.h
 * @brief Binding of the SafeMon reactions to the output drivers.
 */

#ifndef SAFEMON_CFG_H
#define SAFEMON_CFG_H

/**
 * @brief Switches every actuator output to its safe state.
 *
 * @note Callable from any context, including fault handlers on the fault stack.
 */
void SafeMonCfg_SafeOutputs(void);

#endif /* SAFEMON_CFG_H */
