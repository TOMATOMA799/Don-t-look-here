// kmain.c - stage-1 kernel entry. Proves the whole boot chain works by
// printing a heartbeat message over the debug UART, then loops forever.

void uart_init(void);
void uart_puts(const char* s);
void uart_put_hex64(unsigned long v);
int  fb_init(void);
void fb_fill(unsigned int rgb);
void fb_puts(unsigned int x, unsigned int y, const char* s, unsigned int rgb);
void fan_update(void);

void kmain(void) {
    uart_init();
    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts(" Nex OS - stage 1 bootstrap\n");
    uart_puts(" Board:  Raspberry Pi 5 (BCM2712)\n");
    uart_puts(" Status: CPU is executing bare-metal code\n");
    uart_puts("========================================\n");

    uart_puts("[fb] requesting framebuffer via mailbox...\n");
    if (fb_init()) {
        uart_puts("[fb] framebuffer obtained, drawing banner\n");
        fb_fill(0x001a1a2e);              // dark navy background
        fb_puts(60, 60, "HELLO FROM BARE METAL", 0x00e94560);
    } else {
        uart_puts("[fb] framebuffer setup FAILED - HDMI output unavailable, "
                   "continuing on UART only\n");
    }

    unsigned long counter = 0;
    while (1) {
        // Simple heartbeat so you can see it's alive and not hung, without
        // needing a timer/interrupt driver yet.
        for (volatile long i = 0; i < 100000000; i++) { }
        uart_puts("[heartbeat] tick #");
        uart_put_hex64(counter++);
        uart_puts("\n");
        fan_update();
    }
}
