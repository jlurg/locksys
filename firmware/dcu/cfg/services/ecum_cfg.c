/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file ecum_cfg.c
 * @brief Initialisation order of every layer (LS-DCU-SAD-001 section 9.1).
 */

#include "services/ecum_cfg.h"

#include "app/cmd_arb/cmd_arb.h"
#include "app/door_ctrl/door_ctrl.h"
#include "app/mode_mgr/mode_mgr.h"
#include "app/temp_mon/temp_mon.h"
#include "app/win_ctrl/win_ctrl.h"
#include "ecual/cantrcv/cantrcv.h"
#include "ecual/hbridge/hbridge.h"
#include "ecual/winpos/winpos.h"
#include "mcal/adc/adc.h"
#include "mcal/can/can.h"
#include "mcal/dma/dma.h"
#include "mcal/enc/enc.h"
#include "mcal/exti/exti.h"
#include "mcal/gpio/gpio.h"
#include "mcal/i2c/i2c.h"
#include "mcal/pwm/pwm.h"
#include "mcal/pwr/pwr.h"
#include "mcal/uart/uart.h"
#include "rte/rte.h"
#include "services/com/com.h"
#include "services/dcm/dcm.h"
#include "services/dem/dem.h"
#include "services/isotp/isotp.h"
#include "services/tlm/tlm.h"
#include "services/wdgm/wdgm.h"

void EcuMCfg_SafeOutputs(void)
{
    Gpio_InitSafe();
}

void EcuMCfg_InitDrivers(void)
{
    /* The AFIO remap is written before any timer output is enabled. */
    (void)Gpio_Init();
    (void)Exti_Init();
    (void)Dma_Init();
    (void)Adc_Init();
    (void)Pwm_Init();
    (void)Enc_Init();
    (void)Can_Init();
    (void)I2c_Init();
    (void)Uart_Init();
    (void)Pwr_Init();
}

void EcuMCfg_InitEcual(void)
{
    HBridge_Init();
    WinPos_Init();
    (void)CanTrcv_Init();
}

void EcuMCfg_InitServices(void)
{
    Dem_Init();
    IsoTp_Init();
    Dcm_Init();
    Com_Init();
    WdgM_Init();
    Tlm_Init();
    Rte_Init();
}

void EcuMCfg_InitSwcs(void)
{
    CmdArb_Init();
    ModeMgr_Init();
    WinCtrl_Init();
    DoorCtrl_Init();
    TempMon_Init();
}
