/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file ecum_cfg.h
 * @brief Start-up bindings of EcuM: the initialisation order of every layer.
 */

#ifndef ECUM_CFG_H
#define ECUM_CFG_H

/**
 * @brief Step S1: bridge inputs low, EN/DIAG floating inputs, trace pins low.
 */
void EcuMCfg_SafeOutputs(void);

/**
 * @brief Step S6: single AFIO_MAPR write, then the MCAL drivers.
 */
void EcuMCfg_InitDrivers(void);

/**
 * @brief Step S7: ECU abstraction modules.
 */
void EcuMCfg_InitEcual(void);

/**
 * @brief Step S8: services and the RTE.
 */
void EcuMCfg_InitServices(void);

/**
 * @brief Step S8: software components and their engines.
 */
void EcuMCfg_InitSwcs(void);

#endif /* ECUM_CFG_H */
