/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */

/**
 * @file gpio.c
 * @brief Pin configuration, safe initialisation, AFIO remap and pin access.
 */

#include "mcal/gpio/gpio.h"

#include "ls_cfg.h"
#include "mcal/gpio_cfg.h"
#include "platform/ls_static_assert.h"
#include "stm32f1xx.h"

#define GPIO_TRACE_MASK     (0x0Fu)
#define GPIO_BSRR_RESET_POS (16u)
#define GPIO_CR_FIELD_BITS  (4u)
#define GPIO_CR_FIELD_MASK  (0xFu)
#define GPIO_PINS_PER_CR    (8u)

#define GPIO_CLOCKS                                                                      \
    (RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN | \
     RCC_APB2ENR_IOPDEN)

static GPIO_TypeDef *const s_ports[GPIO_PORT_COUNT] = {GPIOA, GPIOB, GPIOC, GPIOD};

static bool s_maprWritten = false;

static GPIO_TypeDef *Gpio_Port(const Gpio_PinCfgType *cfg)
{
    return s_ports[cfg->port];
}

static void Gpio_SetLevel(const Gpio_PinCfgType *cfg, Ls_LevelType level)
{
    const uint32_t bit = 1UL << cfg->pin;

    Gpio_Port(cfg)->BSRR = (level == LS_HIGH) ? bit : (bit << GPIO_BSRR_RESET_POS);
}

static void Gpio_WriteMode(const Gpio_PinCfgType *cfg, Gpio_ModeType mode)
{
    GPIO_TypeDef *const port = Gpio_Port(cfg);
    const uint32_t shift = ((uint32_t)cfg->pin % GPIO_PINS_PER_CR) * GPIO_CR_FIELD_BITS;
    const uint32_t clear = ~(GPIO_CR_FIELD_MASK << shift);
    const uint32_t value = (uint32_t)mode << shift;

    if (cfg->pin < GPIO_PINS_PER_CR)
    {
        port->CRL = (port->CRL & clear) | value;
    }
    else
    {
        port->CRH = (port->CRH & clear) | value;
    }
}

static void Gpio_ConfigurePin(const Gpio_PinCfgType *cfg)
{
    /* Output latch (or pull direction) first, so an output never glitches to the other level. */
    Gpio_SetLevel(cfg, cfg->initLevel);
    Gpio_WriteMode(cfg, cfg->mode);
}

/* @satisfies SWR-DCU-070 */
void Gpio_InitSafe(void)
{
    uint32_t i;

    RCC->APB2ENR |= GPIO_CLOCKS;
    (void)RCC->APB2ENR;
    for (i = 0u; i < GPIO_CFG_SAFE_PIN_COUNT; i++)
    {
        Gpio_ConfigurePin(&GpioCfg_Pins[GpioCfg_SafePins[i]]);
    }
}

/* @satisfies SWR-DCU-070 */
Ls_ReturnType Gpio_Init(void)
{
    uint32_t i;

    if (s_maprWritten)
    {
        return LS_E_NOT_OK;
    }
    /* RM0008 section 9.4.2: SWJ_CFG is write-only; MAPR is written once, never read-modified. */
    AFIO->MAPR = GPIO_CFG_AFIO_MAPR;
    s_maprWritten = true;
    for (i = 0u; i < GPIO_CFG_PIN_COUNT; i++)
    {
        Gpio_ConfigurePin(&GpioCfg_Pins[i]);
    }
    return LS_E_OK;
}

void Gpio_SetMode(Gpio_PinIdType pin, Gpio_ModeType mode)
{
    if (pin < GPIO_CFG_PIN_COUNT)
    {
        Gpio_WriteMode(&GpioCfg_Pins[pin], mode);
    }
}

void Gpio_Write(Gpio_PinIdType pin, Ls_LevelType level)
{
    if (pin < GPIO_CFG_PIN_COUNT)
    {
        Gpio_SetLevel(&GpioCfg_Pins[pin], level);
    }
}

Ls_LevelType Gpio_Read(Gpio_PinIdType pin)
{
    Ls_LevelType level = LS_LOW;

    if (pin < GPIO_CFG_PIN_COUNT)
    {
        const Gpio_PinCfgType *const cfg = &GpioCfg_Pins[pin];
        level = (((Gpio_Port(cfg)->IDR >> cfg->pin) & 1UL) != 0UL) ? LS_HIGH : LS_LOW;
    }
    return level;
}

Ls_LevelType Gpio_ReadOutput(Gpio_PinIdType pin)
{
    Ls_LevelType level = LS_LOW;

    if (pin < GPIO_CFG_PIN_COUNT)
    {
        const Gpio_PinCfgType *const cfg = &GpioCfg_Pins[pin];
        level = (((Gpio_Port(cfg)->ODR >> cfg->pin) & 1UL) != 0UL) ? LS_HIGH : LS_LOW;
    }
    return level;
}

/* @satisfies SWR-DCU-103 */
void Gpio_TraceSet(uint8_t setMask, uint8_t clearMask)
{
#if LS_CFG_TRACE == 1
    /* TRACE0-TRACE3 are PC0-PC3: one BSRR store, set bits win over reset bits. */
    GPIOC->BSRR = ((uint32_t)setMask & GPIO_TRACE_MASK) |
                  (((uint32_t)clearMask & GPIO_TRACE_MASK) << GPIO_BSRR_RESET_POS);
#else
    (void)setMask;
    (void)clearMask;
#endif
}
