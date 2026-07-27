// uart.c - minimal PL011 UART driver for early boot debug output.
//
// *** THE ONE PART OF THIS PROJECT I CANNOT VERIFY WITHOUT REAL HARDWARE ***
//
// On the Pi 5, almost everything changed from earlier Pi boards: most I/O
// (GPIO, the header UART on pins 14/15, etc.) moved off the main BCM2712
// SoC onto a separate companion chip called "RP1", connected over PCIe.
// That means the classic "UART0 at a fixed SoC address" approach used by
// every Pi 3/4 bare-metal tutorial does NOT work as-is on a Pi 5: reading
// RP1's registers before PCIe is initialized causes a CPU data abort
// (a hard crash), and bringing up a PCIe root complex driver just to get
// a UART working is a substantial project on its own.
//
// HOWEVER: the Pi 5 also has a separate, dedicated "debug UART" (a PL011,
// same as older Pis used) wired directly into the BCM2712 SoC itself,
// exposed on a small header near the USB-C power port (NOT the GPIO 14/15
// pins). Community documentation (see README.md for sources) reports it's
// always enabled by the firmware, independent of PCIe/RP1, at physical
// address 0x107D001000. That is the address used below.
//
// I found exactly one corroborating source for this address (a Raspberry
// Pi forum post referencing `earlycon=pl011,0x107d001000`) and could not
// cross-check it against an official register map in this session. This
// is the single riskiest assumption in this whole project. If nothing
// appears on the debug UART header, see README.md's troubleshooting
// section before assuming the rest of the boot chain is broken.

#include <stdint.h>

// FIXED: was 0x107D001000 (missing a hex digit -> unmapped memory).
// Verified against ARM Trusted Firmware's rpi5 platform header
// (plat/rpi/rpi5/include/rpi_hw.h): RPI_IO_BASE (0x1000000000) + 0x7d001000.
#define UART_BASE      0x1007D001000UL

// PL011 register offsets (standard ARM PL011, same layout used on all
// earlier Raspberry Pi boards - this part IS well-documented and stable).
#define UART_DR        (*(volatile uint32_t*)(UART_BASE + 0x00))
#define UART_FR        (*(volatile uint32_t*)(UART_BASE + 0x18))
#define UART_IBRD      (*(volatile uint32_t*)(UART_BASE + 0x24))
#define UART_FBRD      (*(volatile uint32_t*)(UART_BASE + 0x28))
#define UART_LCRH      (*(volatile uint32_t*)(UART_BASE + 0x2C))
#define UART_CR        (*(volatile uint32_t*)(UART_BASE + 0x30))
#define UART_ICR       (*(volatile uint32_t*)(UART_BASE + 0x44))

#define UART_FR_TXFF   (1 << 5)   // transmit FIFO full
#define UART_FR_BUSY   (1 << 3)   // UART busy transmitting

void uart_init(void) {
    // Disable UART while we configure it.
    UART_CR = 0;

    // Clear pending interrupts.
    UART_ICR = 0x7FF;

    // ARM Trusted Firmware's rpi5 platform header documents the PL011
    // reference clock as 44MHz (RPI4_PL011_UART_CLOCK), not 48MHz as
    // previously assumed. For 115200 baud:
    //   divisor = 44000000 / (16*115200) = 23.87...
    //   integer part = 23, fractional part = round(0.87 * 64) = 56
    UART_IBRD = 23;
    UART_FBRD = 56;

    // 8 bits, no parity, 1 stop bit, FIFOs enabled.
    UART_LCRH = (1 << 4) | (3 << 5); // FEN | WLEN=8

    // Enable UART, transmit, and receive.
    UART_CR = (1 << 0) | (1 << 8) | (1 << 9); // UARTEN | TXE | RXE
}

static void uart_putc(char c) {
    while (UART_FR & UART_FR_TXFF) { /* wait for space in TX FIFO */ }
    UART_DR = (uint32_t)c;
}

void uart_puts(const char* s) {
    while (*s) {
        if (*s == '\n') uart_putc('\r'); // CRLF for terminal sanity
        uart_putc(*s++);
    }
}

void uart_put_hex64(uint64_t v) {
    uart_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t nibble = (v >> i) & 0xF;
        uart_putc(nibble < 10 ? ('0' + nibble) : ('a' + nibble - 10));
    }
}
