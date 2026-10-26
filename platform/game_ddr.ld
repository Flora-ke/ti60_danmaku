/* Dedicated-game RAM layout for the inspected newSoC DDR map.
 * Requires exclusive ownership of [0x1000, 0x201000), DDR ready before _start,
 * and a RAM loader honoring all LOAD sections. Not a Flash/XIP layout.
 * Startup symbols match the existing newSoC standalone/common/start.S.
 * tools/build_sapphire.py checks the actual ELF LOAD ranges and owned BSS.
 * Building this RAM layout does not initialize DDR or load a board. */
OUTPUT_ARCH("riscv")
ENTRY(_start)
MEMORY { ram (rwx) : ORIGIN = 0x1000, LENGTH = 2M }
SECTIONS {
    .init : { KEEP(*(.init)) } > ram
    .text : { *(.text .text.*) } > ram
    .rodata : ALIGN(4) { *(.rodata .rodata.* .srodata .srodata.*) } > ram
    .preinit_array : ALIGN(4) {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > ram
    .init_array : ALIGN(4) {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*))) KEEP(*(.init_array))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > ram
    .fini_array : ALIGN(4) {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*))) KEEP(*(.fini_array))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > ram
    .data : ALIGN(8) {
        _data = .;
        *(.data .data.*)
        PROVIDE(__global_pointer$ = . + 0x800);
        *(.sdata .sdata.*)
        . = ALIGN(4);
        _edata = .;
    } > ram
    _data_lma = LOADADDR(.data);
    .bss (NOLOAD) : ALIGN(64) {
        __bss_start = .;
        *(.sbss .sbss.* .bss .bss.*) *(COMMON)
        . = ALIGN(16);
        _end = .; PROVIDE(end = .);
    } > ram
    .stack (NOLOAD) : ALIGN(16) {
        _heap_end = .; _stack_bottom = .;
        . += 64K;
        _sp = .;
        PROVIDE(__freertos_irq_stack_top = .);
    } > ram
    ASSERT(_sp <= ORIGIN(ram) + LENGTH(ram), "Game exceeds reserved 2 MiB DDR")
}
