// mailbox.c - ARM<->VideoCore mailbox property interface.
//
// *** SECOND-RISKIEST PART OF THIS PROJECT, AFTER THE DEBUG UART ***
//
// Base address verified against ARM Trusted Firmware's official rpi5
// platform header (plat/rpi/rpi5/include/rpi_hw.h):
//   RPI_IO_BASE (0x1000000000) + 0x7c013880 = 0x1007c013880
// This is a real firmware source file (not a forum guess), so the base
// address itself is solid. What is NOT solid: whether the property tags
// used below (framebuffer allocation, physical/virtual size, depth,
// pitch) behave identically to Pi 3/4 on the Pi 5's VideoCore VII GPU.
// Community reports from 2024 describe partial success and unresolved
// edge cases getting a mailbox-allocated framebuffer to actually display
// on Pi 5 - there is no confirmed-working reference implementation the
// way there is for earlier Pi boards. If you get a black/garbled screen,
// this file (not boot.S, uart.c, or the linker script) is the suspect.

#include <stdint.h>

#define MBOX_BASE   0x1007c013880UL

#define MBOX_READ    (*(volatile uint32_t*)(MBOX_BASE + 0x00))
#define MBOX_STATUS  (*(volatile uint32_t*)(MBOX_BASE + 0x18))
#define MBOX_WRITE   (*(volatile uint32_t*)(MBOX_BASE + 0x20))

#define MBOX_FULL    0x80000000
#define MBOX_EMPTY   0x40000000
#define MBOX_CH_PROP 8  // ARM-to-VC property channel

int mbox_call(volatile uint32_t* buf) {
    uint32_t addr = ((uint32_t)(uintptr_t)buf & ~0xF) | (MBOX_CH_PROP & 0xF);

    while (MBOX_STATUS & MBOX_FULL) { }
    MBOX_WRITE = addr;

    while (1) {
        while (MBOX_STATUS & MBOX_EMPTY) { }
        uint32_t resp = MBOX_READ;
        if (resp == addr) {
            // buf[1] == 0x80000000 means the firmware processed the
            // whole message successfully.
            return buf[1] == 0x80000000;
        }
    }
}

uint32_t mbox_get_temp_millic(void) {
    static volatile uint32_t buf[8] __attribute__((aligned(16)));
    buf[0] = 8 * 4;      // total size
    buf[1] = 0;          // request code
    buf[2] = 0x00030006; // get temperature
    buf[3] = 8;          // value buffer size (id, value)
    buf[4] = 4;          // request length (just the id word)
    buf[5] = 0;          // id: 0 = CPU thermal sensor
    buf[6] = 0;          // out: temperature in millidegrees C
    buf[7] = 0;          // end tag

    if (!mbox_call(buf)) return 0xFFFFFFFF; // sentinel: read failed
    return buf[6];
}
