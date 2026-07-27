// kmain.c - stage-1 kernel entry. Proves the whole boot chain works by
// printing a heartbeat message over the debug UART, then loops forever.

void uart_init(void);
void uart_puts(const char* s);
void uart_put_hex64(unsigned long v);

void kmain(void) {
    uart_init();
    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts(" Nex OS - stage 1 bootstrap\n");
    uart_puts(" Board:  Raspberry Pi 5 (BCM2712)\n");
    uart_puts(" Status: CPU is executing bare-metal code\n");
    uart_puts("========================================\n");

    unsigned long counter = 0;
    while (1) {
        // Simple heartbeat so you can see it's alive and not hung, without
        // needing a timer/interrupt driver yet.
        for (volatile long i = 0; i < 100000000; i++) { }
        uart_puts("[heartbeat] tick #");
        uart_put_hex64(counter++);
        uart_puts("\n");
    }
}
