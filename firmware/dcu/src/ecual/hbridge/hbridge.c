/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file hbridge.c
 * @brief VNH5019 bridge channels.
 *
 * Skeleton: OFF, the reflex latch and the safe switch-off are implemented; drive, brake,
 * soft start and supervision are delivered at milestones M1 and M2. Until then every drive
 * request is refused and BRAKE is applied as OFF.
 */

#include "ecual/hbridge/hbridge.h"

#include "mcal/gpio/gpio.h"
#include "mcal/gpio_cfg.h"
#include "platform/ls_crit.h"

typedef struct
{
    Gpio_PinIdType pwm;
    Gpio_PinIdType ina;
    Gpio_PinIdType inb;
} HBridge_PinsType;

static const HBridge_PinsType s_pins[HBRIDGE_CH_COUNT] = {
    {GPIO_PIN_WIN_PWM, GPIO_PIN_WIN_INA, GPIO_PIN_WIN_INB},
    {GPIO_PIN_LOCK_PWM, GPIO_PIN_LOCK_INA, GPIO_PIN_LOCK_INB},
};

static HBridge_StatusType s_status[HBRIDGE_CH_COUNT];

static void HBridge_Off(HBridge_ChannelType channel)
{
    Gpio_Write(s_pins[channel].ina, LS_LOW);
    Gpio_Write(s_pins[channel].inb, LS_LOW);
    Gpio_Write(s_pins[channel].pwm, LS_LOW);
    s_status[channel].cmd = HBRIDGE_CMD_OFF;
    s_status[channel].dutyPermille = 0u;
}

void HBridge_Init(void)
{
    HBridge_ChannelType channel;

    for (channel = 0u; channel < HBRIDGE_CH_COUNT; channel++)
    {
        s_status[channel].reflex = 0u;
        s_status[channel].currentMa = 0u;
        s_status[channel].driveOnMs = 0u;
        HBridge_Off(channel);
    }
}

/* @satisfies SWR-DCU-015 */
Ls_ReturnType HBridge_Set(HBridge_ChannelType channel, HBridge_CmdType cmd, uint16_t dutyPermille)
{
    Ls_ReturnType result = LS_E_NOT_OK;
    LsCrit_StateType state;

    (void)dutyPermille;
    if (channel >= HBRIDGE_CH_COUNT)
    {
        return LS_E_NOT_OK;
    }
    state = LsCrit_EnterReflex();
    if ((cmd == HBRIDGE_CMD_OFF) || (cmd == HBRIDGE_CMD_BRAKE))
    {
        HBridge_Off(channel);
        result = LS_E_OK;
    }
    LsCrit_Exit(state);
    return result;
}

void HBridge_ReflexStop(HBridge_ChannelType channel, HBridge_ReflexType cause)
{
    if (channel < HBRIDGE_CH_COUNT)
    {
        const LsCrit_StateType state = LsCrit_EnterReflex();
        s_status[channel].reflex |= cause;
        HBridge_Off(channel);
        LsCrit_Exit(state);
    }
}

void HBridge_DiagIsr(void)
{
    HBridge_ReflexStop(HBRIDGE_CH_WIN, HBRIDGE_REFLEX_DIAG);
}

void HBridge_ClearReflex(HBridge_ChannelType channel, HBridge_ReflexType causes)
{
    if (channel < HBRIDGE_CH_COUNT)
    {
        const LsCrit_StateType state = LsCrit_EnterReflex();
        s_status[channel].reflex &= (HBridge_ReflexType)(~causes | HBRIDGE_REFLEX_HANG);
        LsCrit_Exit(state);
    }
}

/* @satisfies SWR-DCU-009 */
void HBridge_AllOff(void)
{
    HBridge_Off(HBRIDGE_CH_WIN);
    HBridge_Off(HBRIDGE_CH_LOCK);
}

void HBridge_Main1ms(void)
{
}

Ls_ReturnType HBridge_GetStatus(HBridge_ChannelType channel, HBridge_StatusType *status)
{
    if ((channel >= HBRIDGE_CH_COUNT) || (status == NULL))
    {
        return LS_E_NOT_OK;
    }
    *status = s_status[channel];
    return LS_E_OK;
}
