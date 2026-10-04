;/* SPDX-License-Identifier: Apache-2.0 */
;/* Copyright (c) 2026 jlurg */
;
; Start-up code of the DCU for IAR EWARM: vector table of the STM32F103xB and reset sequence,
; derived from the STMicroelectronics CMSIS device template
; third_party/cmsis_device_f1-4.3.5/Source/Templates/iar/startup_stm32f103xb.s
; (Copyright (c) 2017-2021 STMicroelectronics, Apache-2.0).
;
; Changes to the template: the reset handler calls EcuM_EarlyInit (safe outputs, start-up step
; S1; uses no RAM data) instead of SystemInit, then __iar_program_start. Fault exceptions and
; unexpected interrupts enter SafeMon_FaultEntry on FAULT_STACK; the stack pointer is loaded
; through R0 because ES096 section 2.1.3 excludes loads to SP from memory.

        MODULE  ?cstartup

        SECTION CSTACK:DATA:NOROOT(3)
        SECTION FAULT_STACK:DATA:NOROOT(3)

        SECTION .intvec:CODE:NOROOT(2)

        EXTERN  __iar_program_start
        EXTERN  EcuM_EarlyInit
        EXTERN  SafeMon_FaultEntry
        PUBLIC  __vector_table

        DATA
__vector_table
        DCD     sfe(CSTACK)
        DCD     Reset_Handler
        DCD     NMI_Handler
        DCD     HardFault_Handler
        DCD     MemManage_Handler
        DCD     BusFault_Handler
        DCD     UsageFault_Handler
        DCD     0
        DCD     0
        DCD     0
        DCD     0
        DCD     SVC_Handler
        DCD     DebugMon_Handler
        DCD     0
        DCD     PendSV_Handler
        DCD     SysTick_Handler
        DCD     WWDG_IRQHandler
        DCD     PVD_IRQHandler
        DCD     TAMPER_IRQHandler
        DCD     RTC_IRQHandler
        DCD     FLASH_IRQHandler
        DCD     RCC_IRQHandler
        DCD     EXTI0_IRQHandler
        DCD     EXTI1_IRQHandler
        DCD     EXTI2_IRQHandler
        DCD     EXTI3_IRQHandler
        DCD     EXTI4_IRQHandler
        DCD     DMA1_Channel1_IRQHandler
        DCD     DMA1_Channel2_IRQHandler
        DCD     DMA1_Channel3_IRQHandler
        DCD     DMA1_Channel4_IRQHandler
        DCD     DMA1_Channel5_IRQHandler
        DCD     DMA1_Channel6_IRQHandler
        DCD     DMA1_Channel7_IRQHandler
        DCD     ADC1_2_IRQHandler
        DCD     USB_HP_CAN1_TX_IRQHandler
        DCD     USB_LP_CAN1_RX0_IRQHandler
        DCD     CAN1_RX1_IRQHandler
        DCD     CAN1_SCE_IRQHandler
        DCD     EXTI9_5_IRQHandler
        DCD     TIM1_BRK_IRQHandler
        DCD     TIM1_UP_IRQHandler
        DCD     TIM1_TRG_COM_IRQHandler
        DCD     TIM1_CC_IRQHandler
        DCD     TIM2_IRQHandler
        DCD     TIM3_IRQHandler
        DCD     TIM4_IRQHandler
        DCD     I2C1_EV_IRQHandler
        DCD     I2C1_ER_IRQHandler
        DCD     I2C2_EV_IRQHandler
        DCD     I2C2_ER_IRQHandler
        DCD     SPI1_IRQHandler
        DCD     SPI2_IRQHandler
        DCD     USART1_IRQHandler
        DCD     USART2_IRQHandler
        DCD     USART3_IRQHandler
        DCD     EXTI15_10_IRQHandler
        DCD     RTC_Alarm_IRQHandler
        DCD     USBWakeUp_IRQHandler

        THUMB

        PUBWEAK Reset_Handler
        SECTION .text:CODE:REORDER:NOROOT(2)
Reset_Handler
        LDR     R0, =EcuM_EarlyInit
        BLX     R0
        LDR     R0, =__iar_program_start
        BX      R0

        SECTION .text:CODE:REORDER:NOROOT(2)
Ls_FaultStub
        LDR     R0, =sfe(FAULT_STACK)
        MOV     SP, R0
        LDR     R1, =SafeMon_FaultEntry
        BX      R1

        PUBLIC  NMI_Handler
        SECTION .text:CODE:REORDER:NOROOT(1)
NMI_Handler
        B       Ls_FaultStub

        PUBLIC  HardFault_Handler
        SECTION .text:CODE:REORDER:NOROOT(1)
HardFault_Handler
        B       Ls_FaultStub

        PUBLIC  MemManage_Handler
        SECTION .text:CODE:REORDER:NOROOT(1)
MemManage_Handler
        B       Ls_FaultStub

        PUBLIC  BusFault_Handler
        SECTION .text:CODE:REORDER:NOROOT(1)
BusFault_Handler
        B       Ls_FaultStub

        PUBLIC  UsageFault_Handler
        SECTION .text:CODE:REORDER:NOROOT(1)
UsageFault_Handler
        B       Ls_FaultStub

        PUBWEAK SVC_Handler
        SECTION .text:CODE:REORDER:NOROOT(1)
SVC_Handler
        B       Ls_FaultStub

        PUBWEAK DebugMon_Handler
        SECTION .text:CODE:REORDER:NOROOT(1)
DebugMon_Handler
        B       Ls_FaultStub

        PUBWEAK PendSV_Handler
        SECTION .text:CODE:REORDER:NOROOT(1)
PendSV_Handler
        B       Ls_FaultStub

        PUBWEAK SysTick_Handler
        SECTION .text:CODE:REORDER:NOROOT(1)
SysTick_Handler
        B       Ls_FaultStub

        PUBWEAK WWDG_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
WWDG_IRQHandler
        B       Ls_FaultStub

        PUBWEAK PVD_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
PVD_IRQHandler
        B       Ls_FaultStub

        PUBWEAK TAMPER_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
TAMPER_IRQHandler
        B       Ls_FaultStub

        PUBWEAK RTC_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
RTC_IRQHandler
        B       Ls_FaultStub

        PUBWEAK FLASH_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
FLASH_IRQHandler
        B       Ls_FaultStub

        PUBWEAK RCC_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
RCC_IRQHandler
        B       Ls_FaultStub

        PUBWEAK EXTI0_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
EXTI0_IRQHandler
        B       Ls_FaultStub

        PUBWEAK EXTI1_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
EXTI1_IRQHandler
        B       Ls_FaultStub

        PUBWEAK EXTI2_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
EXTI2_IRQHandler
        B       Ls_FaultStub

        PUBWEAK EXTI3_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
EXTI3_IRQHandler
        B       Ls_FaultStub

        PUBWEAK EXTI4_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
EXTI4_IRQHandler
        B       Ls_FaultStub

        PUBWEAK DMA1_Channel1_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
DMA1_Channel1_IRQHandler
        B       Ls_FaultStub

        PUBWEAK DMA1_Channel2_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
DMA1_Channel2_IRQHandler
        B       Ls_FaultStub

        PUBWEAK DMA1_Channel3_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
DMA1_Channel3_IRQHandler
        B       Ls_FaultStub

        PUBWEAK DMA1_Channel4_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
DMA1_Channel4_IRQHandler
        B       Ls_FaultStub

        PUBWEAK DMA1_Channel5_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
DMA1_Channel5_IRQHandler
        B       Ls_FaultStub

        PUBWEAK DMA1_Channel6_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
DMA1_Channel6_IRQHandler
        B       Ls_FaultStub

        PUBWEAK DMA1_Channel7_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
DMA1_Channel7_IRQHandler
        B       Ls_FaultStub

        PUBWEAK ADC1_2_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
ADC1_2_IRQHandler
        B       Ls_FaultStub

        PUBWEAK USB_HP_CAN1_TX_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
USB_HP_CAN1_TX_IRQHandler
        B       Ls_FaultStub

        PUBWEAK USB_LP_CAN1_RX0_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
USB_LP_CAN1_RX0_IRQHandler
        B       Ls_FaultStub

        PUBWEAK CAN1_RX1_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
CAN1_RX1_IRQHandler
        B       Ls_FaultStub

        PUBWEAK CAN1_SCE_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
CAN1_SCE_IRQHandler
        B       Ls_FaultStub

        PUBWEAK EXTI9_5_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
EXTI9_5_IRQHandler
        B       Ls_FaultStub

        PUBWEAK TIM1_BRK_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
TIM1_BRK_IRQHandler
        B       Ls_FaultStub

        PUBWEAK TIM1_UP_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
TIM1_UP_IRQHandler
        B       Ls_FaultStub

        PUBWEAK TIM1_TRG_COM_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
TIM1_TRG_COM_IRQHandler
        B       Ls_FaultStub

        PUBWEAK TIM1_CC_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
TIM1_CC_IRQHandler
        B       Ls_FaultStub

        PUBWEAK TIM2_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
TIM2_IRQHandler
        B       Ls_FaultStub

        PUBWEAK TIM3_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
TIM3_IRQHandler
        B       Ls_FaultStub

        PUBWEAK TIM4_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
TIM4_IRQHandler
        B       Ls_FaultStub

        PUBWEAK I2C1_EV_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
I2C1_EV_IRQHandler
        B       Ls_FaultStub

        PUBWEAK I2C1_ER_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
I2C1_ER_IRQHandler
        B       Ls_FaultStub

        PUBWEAK I2C2_EV_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
I2C2_EV_IRQHandler
        B       Ls_FaultStub

        PUBWEAK I2C2_ER_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
I2C2_ER_IRQHandler
        B       Ls_FaultStub

        PUBWEAK SPI1_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
SPI1_IRQHandler
        B       Ls_FaultStub

        PUBWEAK SPI2_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
SPI2_IRQHandler
        B       Ls_FaultStub

        PUBWEAK USART1_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
USART1_IRQHandler
        B       Ls_FaultStub

        PUBWEAK USART2_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
USART2_IRQHandler
        B       Ls_FaultStub

        PUBWEAK USART3_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
USART3_IRQHandler
        B       Ls_FaultStub

        PUBWEAK EXTI15_10_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
EXTI15_10_IRQHandler
        B       Ls_FaultStub

        PUBWEAK RTC_Alarm_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
RTC_Alarm_IRQHandler
        B       Ls_FaultStub

        PUBWEAK USBWakeUp_IRQHandler
        SECTION .text:CODE:REORDER:NOROOT(1)
USBWakeUp_IRQHandler
        B       Ls_FaultStub

        END
