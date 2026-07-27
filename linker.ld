/* linker.ld - places our image at 0x80000, the load address the Pi 5
 * firmware uses for a bare-metal/non-Linux kernel8.img when os_check=0
 * is set in config.txt. */

ENTRY(_start)

SECTIONS
{
    . = 0x80000;

    .text.boot : { KEEP(*(.text.boot)) }
    .text      : { *(.text .text.*) }
    .rodata    : { *(.rodata .rodata.*) }
    .data      : { *(.data .data.*) }

    . = ALIGN(8);
    __bss_start = .;
    .bss (NOLOAD) : { *(.bss .bss.* COMMON) }
    __bss_end = .;

    /DISCARD/ : { *(.comment) *(.note.*) *(.eh_frame) }
}
