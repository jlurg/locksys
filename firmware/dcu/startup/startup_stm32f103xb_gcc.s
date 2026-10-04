/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */
/*
 * Start-up code of the DCU for the GNU toolchain (GCC shadow build): vector table of the
 * STM32F103xB (vector order as in the STMicroelectronics CMSIS device template,
 * third_party/cmsis_device_f1-4.3.5/Source/Templates/gcc/startup_stm32f103xb.s), reset
 * sequence and the fault entry stub.
 *
 * Reset sequence: EcuM_EarlyInit() (safe outputs, start-up step S1; uses no RAM data), copy of
 * .data, zeroing of .bss, main(). The fault entry loads the fault stack pointer into R0 and
 * moves it to SP; ES096 section 2.1.3 excludes loads to SP from memory.
 */

    .syntax unified
    .cpu cortex-m3
    .thumb

    .global g_pfnVectors
    .global Default_Handler
    .global Reset_Handler

    .section .text.Reset_Handler, "ax", %progbits
    .type Reset_Handler, %function
Reset_Handler:
    bl      EcuM_EarlyInit

    ldr     r0, =_sdata
    ldr     r1, =_edata
    ldr     r2, =_sidata
    b       2f
1:
    ldr     r3, [r2], #4
    str     r3, [r0], #4
2:
    cmp     r0, r1
    bcc     1b

    ldr     r0, =_sbss
    ldr     r1, =_ebss
    movs    r2, #0
    b       4f
3:
    str     r2, [r0], #4
4:
    cmp     r0, r1
    bcc     3b

    bl      main
5:
    b       5b
    .size Reset_Handler, . - Reset_Handler

/* Fault and unexpected-interrupt entry: switch to the fault stack, then SafeMon. */
    .section .text.Ls_FaultStub, "ax", %progbits
    .type Ls_FaultStub, %function
Ls_FaultStub:
    ldr     r0, =_fault_stack_top
    mov     sp, r0
    b       SafeMon_FaultEntry
    .size Ls_FaultStub, . - Ls_FaultStub

    .section .text.Default_Handler, "ax", %progbits
    .type Default_Handler, %function
Default_Handler:
    b       Ls_FaultStub
    .size Default_Handler, . - Default_Handler

/* ROM CRC placeholder; the post-link step writes the CRC-32 of 0x08000000-0x0801EFFB. */
    .section .checksum, "a", %progbits
    .global ls_rom_crc
    .type ls_rom_crc, %object
ls_rom_crc:
    .word   0xFFFFFFFF
    .size ls_rom_crc, . - ls_rom_crc

    .section .isr_vector, "a", %progbits
    .type g_pfnVectors, %object
g_pfnVectors:
    .word   _estack
    .word   Reset_Handler
    .word   NMI_Handler
    .word   HardFault_Handler
    .word   MemManage_Handler
    .word   BusFault_Handler
    .word   UsageFault_Handler
    .word   0
    .word   0
    .word   0
    .word   0
    .word   SVC_Handler
    .word   DebugMon_Handler
    .word   0
    .word   PendSV_Handler
    .word   SysTick_Handler
    .word   WWDG_IRQHandler
    .word   PVD_IRQHandler
    .word   TAMPER_IRQHandler
    .word   RTC_IRQHandler
    .word   FLASH_IRQHandler
    .word   RCC_IRQHandler
    .word   EXTI0_IRQHandler
    .word   EXTI1_IRQHandler
    .word   EXTI2_IRQHandler
    .word   EXTI3_IRQHandler
    .word   EXTI4_IRQHandler
    .word   DMA1_Channel1_IRQHandler
    .word   DMA1_Channel2_IRQHandler
    .word   DMA1_Channel3_IRQHandler
    .word   DMA1_Channel4_IRQHandler
    .word   DMA1_Channel5_IRQHandler
    .word   DMA1_Channel6_IRQHandler
    .word   DMA1_Channel7_IRQHandler
    .word   ADC1_2_IRQHandler
    .word   USB_HP_CAN1_TX_IRQHandler
    .word   USB_LP_CAN1_RX0_IRQHandler
    .word   CAN1_RX1_IRQHandler
    .word   CAN1_SCE_IRQHandler
    .word   EXTI9_5_IRQHandler
    .word   TIM1_BRK_IRQHandler
    .word   TIM1_UP_IRQHandler
    .word   TIM1_TRG_COM_IRQHandler
    .word   TIM1_CC_IRQHandler
    .word   TIM2_IRQHandler
    .word   TIM3_IRQHandler
    .word   TIM4_IRQHandler
    .word   I2C1_EV_IRQHandler
    .word   I2C1_ER_IRQHandler
    .word   I2C2_EV_IRQHandler
    .word   I2C2_ER_IRQHandler
    .word   SPI1_IRQHandler
    .word   SPI2_IRQHandler
    .word   USART1_IRQHandler
    .word   USART2_IRQHandler
    .word   USART3_IRQHandler
    .word   EXTI15_10_IRQHandler
    .word   RTC_Alarm_IRQHandler
    .word   USBWakeUp_IRQHandler
    .size g_pfnVectors, . - g_pfnVectors

/* Fault exceptions always use the fault stub. */
    .thumb_set NMI_Handler, Ls_FaultStub
    .thumb_set HardFault_Handler, Ls_FaultStub
    .thumb_set MemManage_Handler, Ls_FaultStub
    .thumb_set BusFault_Handler, Ls_FaultStub
    .thumb_set UsageFault_Handler, Ls_FaultStub

/* Handlers not provided by a driver default to the fault stub. */
    .weak SVC_Handler
    .thumb_set SVC_Handler, Default_Handler
    .weak DebugMon_Handler
    .thumb_set DebugMon_Handler, Default_Handler
    .weak PendSV_Handler
    .thumb_set PendSV_Handler, Default_Handler
    .weak SysTick_Handler
    .thumb_set SysTick_Handler, Default_Handler
    .weak WWDG_IRQHandler
    .thumb_set WWDG_IRQHandler, Default_Handler
    .weak PVD_IRQHandler
    .thumb_set PVD_IRQHandler, Default_Handler
    .weak TAMPER_IRQHandler
    .thumb_set TAMPER_IRQHandler, Default_Handler
    .weak RTC_IRQHandler
    .thumb_set RTC_IRQHandler, Default_Handler
    .weak FLASH_IRQHandler
    .thumb_set FLASH_IRQHandler, Default_Handler
    .weak RCC_IRQHandler
    .thumb_set RCC_IRQHandler, Default_Handler
    .weak EXTI0_IRQHandler
    .thumb_set EXTI0_IRQHandler, Default_Handler
    .weak EXTI1_IRQHandler
    .thumb_set EXTI1_IRQHandler, Default_Handler
    .weak EXTI2_IRQHandler
    .thumb_set EXTI2_IRQHandler, Default_Handler
    .weak EXTI3_IRQHandler
    .thumb_set EXTI3_IRQHandler, Default_Handler
    .weak EXTI4_IRQHandler
    .thumb_set EXTI4_IRQHandler, Default_Handler
    .weak DMA1_Channel1_IRQHandler
    .thumb_set DMA1_Channel1_IRQHandler, Default_Handler
    .weak DMA1_Channel2_IRQHandler
    .thumb_set DMA1_Channel2_IRQHandler, Default_Handler
    .weak DMA1_Channel3_IRQHandler
    .thumb_set DMA1_Channel3_IRQHandler, Default_Handler
    .weak DMA1_Channel4_IRQHandler
    .thumb_set DMA1_Channel4_IRQHandler, Default_Handler
    .weak DMA1_Channel5_IRQHandler
    .thumb_set DMA1_Channel5_IRQHandler, Default_Handler
    .weak DMA1_Channel6_IRQHandler
    .thumb_set DMA1_Channel6_IRQHandler, Default_Handler
    .weak DMA1_Channel7_IRQHandler
    .thumb_set DMA1_Channel7_IRQHandler, Default_Handler
    .weak ADC1_2_IRQHandler
    .thumb_set ADC1_2_IRQHandler, Default_Handler
    .weak USB_HP_CAN1_TX_IRQHandler
    .thumb_set USB_HP_CAN1_TX_IRQHandler, Default_Handler
    .weak USB_LP_CAN1_RX0_IRQHandler
    .thumb_set USB_LP_CAN1_RX0_IRQHandler, Default_Handler
    .weak CAN1_RX1_IRQHandler
    .thumb_set CAN1_RX1_IRQHandler, Default_Handler
    .weak CAN1_SCE_IRQHandler
    .thumb_set CAN1_SCE_IRQHandler, Default_Handler
    .weak EXTI9_5_IRQHandler
    .thumb_set EXTI9_5_IRQHandler, Default_Handler
    .weak TIM1_BRK_IRQHandler
    .thumb_set TIM1_BRK_IRQHandler, Default_Handler
    .weak TIM1_UP_IRQHandler
    .thumb_set TIM1_UP_IRQHandler, Default_Handler
    .weak TIM1_TRG_COM_IRQHandler
    .thumb_set TIM1_TRG_COM_IRQHandler, Default_Handler
    .weak TIM1_CC_IRQHandler
    .thumb_set TIM1_CC_IRQHandler, Default_Handler
    .weak TIM2_IRQHandler
    .thumb_set TIM2_IRQHandler, Default_Handler
    .weak TIM3_IRQHandler
    .thumb_set TIM3_IRQHandler, Default_Handler
    .weak TIM4_IRQHandler
    .thumb_set TIM4_IRQHandler, Default_Handler
    .weak I2C1_EV_IRQHandler
    .thumb_set I2C1_EV_IRQHandler, Default_Handler
    .weak I2C1_ER_IRQHandler
    .thumb_set I2C1_ER_IRQHandler, Default_Handler
    .weak I2C2_EV_IRQHandler
    .thumb_set I2C2_EV_IRQHandler, Default_Handler
    .weak I2C2_ER_IRQHandler
    .thumb_set I2C2_ER_IRQHandler, Default_Handler
    .weak SPI1_IRQHandler
    .thumb_set SPI1_IRQHandler, Default_Handler
    .weak SPI2_IRQHandler
    .thumb_set SPI2_IRQHandler, Default_Handler
    .weak USART1_IRQHandler
    .thumb_set USART1_IRQHandler, Default_Handler
    .weak USART2_IRQHandler
    .thumb_set USART2_IRQHandler, Default_Handler
    .weak USART3_IRQHandler
    .thumb_set USART3_IRQHandler, Default_Handler
    .weak EXTI15_10_IRQHandler
    .thumb_set EXTI15_10_IRQHandler, Default_Handler
    .weak RTC_Alarm_IRQHandler
    .thumb_set RTC_Alarm_IRQHandler, Default_Handler
    .weak USBWakeUp_IRQHandler
    .thumb_set USBWakeUp_IRQHandler, Default_Handler
