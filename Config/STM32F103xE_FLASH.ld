/*
 * Linker script for STM32F103xE (high-density, 512 KB Flash, 64 KB SRAM)
 * GCC toolchain
 */

ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 512K
    RAM  (rwx) : ORIGIN = 0x20000000, LENGTH =  64K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);    /* top of stack */

SECTIONS
{
    /* ---- Vector table (must be at FLASH start) ---- */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    /* ---- Code ---- */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } >FLASH

    /* ---- Read-only data ---- */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

    /* ---- Initialized data (loaded from FLASH, run in RAM) ---- */
    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >RAM AT> FLASH

    /* ---- Uninitialized data (BSS) ---- */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } >RAM
}
