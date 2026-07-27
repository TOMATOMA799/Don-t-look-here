// boot.S - Nex OS stage-1 bootstrap for Raspberry Pi 5 (BCM2712, Cortex-A76, AArch64)
//
// This is the FIRST CODE THAT RUNS on the CPU cores after the Pi 5's on-board
// EEPROM bootloader + firmware have loaded this image from the SD card and
// jumped to it (per config.txt: kernel=kernel8.img, os_check=0, arm_64bit=1).
//
// Load address: 0x80000 (this is where the Pi 5 firmware places the kernel
// image when os_check=0 is set in config.txt for non-Linux/bare-metal boot -
// with os_check left at its default, it instead loads at 0x200000 expecting
// a Linux-style boot, which is NOT what we want here).
//
// All 4 Cortex-A76 cores start executing this same image simultaneously.
// We must park cores 1-3 in a low-power wait loop and let only core 0
// (primary) proceed, since we have no scheduler yet.

.section ".text.boot"
.global _start

_start:
    // Which core am I? MPIDR_EL1 low bits = core ID (Aff0 field).
    mrs     x1, mpidr_el1
    and     x1, x1, #0xFF
    cbz     x1, primary_core        // core 0 -> continue booting
    // Cores 1,2,3: park forever in low-power wait-for-event loop.
park_loop:
    wfe
    b       park_loop

primary_core:
    // Set up the stack pointer just below our load address (grows down
    // from 0x80000, i.e. stack lives in the memory below the kernel image).
    ldr     x1, =_start
    mov     sp, x1

    // Zero the .bss section (uninitialized globals/statics) - required
    // because C code assumes zero-initialized globals start at 0.
    ldr     x1, =__bss_start
    ldr     x2, =__bss_end
zero_bss:
    cmp     x1, x2
    b.ge    bss_done
    str     xzr, [x1], #8
    b       zero_bss
bss_done:

    // Jump into C. x0 may hold a device-tree pointer from firmware but we
    // ignore it for now (os_check=0 bare-metal boot doesn't guarantee one).
    bl      kmain

    // kmain should never return, but if it does, park forever.
hang:
    wfe
    b       hang
