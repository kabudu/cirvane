/*
 * ESP32-C5 spike image. SRAM holds the spike; two flash-mapped segments
 * satisfy the C5 bootloader (app desc + unpack_load_app rom_index == 2).
 *
 * SRAM window matches ESP-IDF esp32c5 memory.ld.in without TEE:
 * 0x40800000 .. 0x4084E5A0 (bootloader iram_loader_seg starts at the end).
 *
 * PHDR order is DROM, IROM, SRAM so segment 0 begins with esp_app_desc_t
 * (magic 0xABCD5432). DROM is padded so IROM sits on the next 64 KB MMU
 * page with paddr%64K == vaddr%64K == 0x20.
 */

MEMORY
{
    drom (r)   : ORIGIN = 0x42000020, LENGTH = 0x10000
    irom (r)   : ORIGIN = 0x42010020, LENGTH = 0x100
    sram (rwx) : ORIGIN = 0x40800000, LENGTH = 0x4E000
}

ENTRY(_start)

PHDRS
{
    drom PT_LOAD;
    irom PT_LOAD;
    sram PT_LOAD;
}

SECTIONS
{
    .drom :
    {
        KEEP(*(.appdesc))
        . = ORIGIN(drom) + 0xFFF8;
    } > drom : drom

    .irom :
    {
        KEEP(*(.iromdummy))
    } > irom : irom

    .text : ALIGN(16)
    {
        KEEP(*(.text.start))
        *(.text .text.*)
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
        . = ALIGN(16);
        _edata_load = .;
    } > sram : sram

    .data : ALIGN(16)
    {
        *(.data .data.*)
        *(.sdata .sdata.*)
        . = ALIGN(16);
    } > sram : sram

    .bss (NOLOAD) : ALIGN(16)
    {
        _sbss = .;
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
        . = ALIGN(16);
        _ebss = .;
    } > sram : sram

    _stack_top = ORIGIN(sram) + LENGTH(sram);
    PROVIDE(_end = .);
}
