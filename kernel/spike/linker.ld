/*
 * ESP32-C5 SRAM-only spike image. The IDF second-stage bootloader copies
 * this into internal SRAM and jumps to _start. No FreeRTOS, no flash XIP.
 *
 * SRAM window matches ESP-IDF esp32c5 memory.ld.in without TEE:
 * 0x40800000 .. 0x4084E5A0 (bootloader iram_loader_seg starts at the end).
 */

MEMORY
{
    sram (rwx) : ORIGIN = 0x40800000, LENGTH = 0x4E000
}

ENTRY(_start)

SECTIONS
{
    .text : ALIGN(16)
    {
        KEEP(*(.text.start))
        *(.text .text.*)
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
        . = ALIGN(16);
        _edata_load = .;
    } > sram

    .data : ALIGN(16)
    {
        *(.data .data.*)
        *(.sdata .sdata.*)
        . = ALIGN(16);
    } > sram

    .bss (NOLOAD) : ALIGN(16)
    {
        _sbss = .;
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
        . = ALIGN(16);
        _ebss = .;
    } > sram

    _stack_top = ORIGIN(sram) + LENGTH(sram);
    PROVIDE(_end = .);
}
