/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 jlurg */
/*
 * GNU linker script of the DCU (GCC shadow build), memory map of LS-DCU-SAD-001 section 11.1:
 * application flash 0x08000000-0x0801EFFB, ROM CRC word ls_rom_crc at 0x0801EFFC, reserved flash
 * 0x0801F000-0x0801FFFF; CSTACK 2 KB at the bottom of SRAM, noinit 256 B above it, then data,
 * bss and FAULT_STACK. No heap.
 */

ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)   : ORIGIN = 0x08000000, LENGTH = 0x1EFFC
    ROMCRC (r)   : ORIGIN = 0x0801EFFC, LENGTH = 4
    CSTACK (rw)  : ORIGIN = 0x20000000, LENGTH = 2K
    NOINIT (rw)  : ORIGIN = 0x20000800, LENGTH = 256
    RAM (rwx)    : ORIGIN = 0x20000900, LENGTH = 20K - 2K - 256
}

_estack = ORIGIN(CSTACK) + LENGTH(CSTACK);
_cstack_start = ORIGIN(CSTACK);

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        *(.glue_7)
        *(.glue_7t)
        . = ALIGN(4);
    } > FLASH

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > FLASH

    _sidata = LOADADDR(.data);

    .checksum :
    {
        KEEP(*(.checksum))
    } > ROMCRC

    .noinit (NOLOAD) :
    {
        . = ALIGN(4);
        *(.noinit)
        *(.noinit*)
        . = ALIGN(4);
    } > NOINIT

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    .fault_stack (NOLOAD) :
    {
        . = ALIGN(8);
        . = . + 128;
        _fault_stack_top = .;
    } > RAM

    .ARM.attributes 0 : { *(.ARM.attributes) }
}

ASSERT(_ebss <= ORIGIN(RAM) + LENGTH(RAM), "RAM overflow")

/* Resource gates of SWR-DCU-111: flash <= 75 % of the application region, RAM <= 60 %. */
_ls_flash_used = SIZEOF(.isr_vector) + SIZEOF(.text) + SIZEOF(.ARM.exidx) + SIZEOF(.data);
_ls_ram_used = LENGTH(CSTACK) + LENGTH(NOINIT) + (_fault_stack_top - ORIGIN(RAM));
ASSERT(_ls_flash_used * 4 <= LENGTH(FLASH) * 3, "flash use above 75 % of the application region")
ASSERT(_ls_ram_used * 5 <= (20 * 1024) * 3, "RAM use above 60 %")
