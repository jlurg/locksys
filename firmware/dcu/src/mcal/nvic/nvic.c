/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file nvic.c
 * @brief Interrupt priorities, fault enables and fault information.
 */

#include "mcal/nvic/nvic.h"

#include "ls_cfg.h"
#include "stm32f1xx.h"

/* PRIGROUP = 3: 4 preemption bits, no sub-priority. */
#define NVIC_PRIGROUP_4_BITS (3u)

#define NVIC_PRIO_I2C      (0u)
#define NVIC_PRIO_REFLEX   (1u)
#define NVIC_PRIO_SYSTICK  (2u)
#define NVIC_PRIO_CAN_RX   (3u)
#define NVIC_PRIO_CAN_TX   (4u)
#define NVIC_PRIO_DMA_UART (5u)

typedef struct
{
    IRQn_Type irq;
    uint8_t priority;
} Nvic_PrioCfgType;

static const Nvic_PrioCfgType s_prio[] = {
    {BusFault_IRQn, 0u},
    {UsageFault_IRQn, 0u},
    {I2C1_EV_IRQn, NVIC_PRIO_I2C},
    {I2C1_ER_IRQn, NVIC_PRIO_I2C},
    {EXTI15_10_IRQn, NVIC_PRIO_REFLEX},
#if LS_CFG_LOCK_DIAG_OPTION_B == 1
    {EXTI4_IRQn, NVIC_PRIO_REFLEX},
#else
    {EXTI9_5_IRQn, NVIC_PRIO_REFLEX},
#endif
    {PVD_IRQn, NVIC_PRIO_REFLEX},
    {SysTick_IRQn, NVIC_PRIO_SYSTICK},
    {USB_LP_CAN1_RX0_IRQn, NVIC_PRIO_CAN_RX},
    {CAN1_RX1_IRQn, NVIC_PRIO_CAN_RX},
    {CAN1_SCE_IRQn, NVIC_PRIO_CAN_RX},
    {USB_HP_CAN1_TX_IRQn, NVIC_PRIO_CAN_TX},
    {DMA1_Channel7_IRQn, NVIC_PRIO_DMA_UART},
};

void Nvic_Init(void)
{
    uint32_t i;

    NVIC_SetPriorityGrouping(NVIC_PRIGROUP_4_BITS);
    for (i = 0u; i < (sizeof(s_prio) / sizeof(s_prio[0])); i++)
    {
        NVIC_SetPriority(s_prio[i].irq, s_prio[i].priority);
    }
    SCB->SHCSR |= SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_USGFAULTENA_Msk;
}

void Nvic_GetFaultInfo(Nvic_FaultInfoType *info)
{
    if (info != NULL)
    {
        info->cfsr = SCB->CFSR;
        info->hfsr = SCB->HFSR;
        info->bfar = SCB->BFAR;
    }
}

bool Nvic_IsDebuggerAttached(void)
{
    return ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) != 0UL);
}

void Nvic_SystemReset(void)
{
    NVIC_SystemReset();
}
