// fan.c - fan speed curve logic for the Pi 5 official case fan.
//
// STATUS: the curve math below works today and is fully testable. The
// actual PWM write at the bottom (fan_apply) is a STUB - it cannot do
// anything real yet, because the case fan's PWM line is wired through
// RP1, and RP1 registers are only safely reachable after PCIe root-
// complex bring-up (see README.md's phased roadmap: UART -> timers ->
// RP1/PCIe -> GPIO -> fan/storage/network). Wire fan_apply() up to the
// real RP1 PWM peripheral once that driver exists; until then this file
// just proves the curve logic is right and reports what it *would* do
// over UART.

#include <stdint.h>
#include "mailbox.h"

void uart_puts(const char* s);
void uart_put_hex64(unsigned long v);

#define FAN_MIN_PERCENT   10   // always-on floor, never goes below this
#define FAN_MAX_PERCENT   100
#define FAN_TEMP_LOW_C    40   // at/below this: floor speed
#define FAN_TEMP_HIGH_C   75   // at/above this: full speed

// Linear ramp between FAN_TEMP_LOW_C (-> 10%) and FAN_TEMP_HIGH_C (-> 100%),
// clamped at both ends. temp_c is whole degrees Celsius.
static unsigned fan_curve_percent(int temp_c) {
    if (temp_c <= FAN_TEMP_LOW_C)  return FAN_MIN_PERCENT;
    if (temp_c >= FAN_TEMP_HIGH_C) return FAN_MAX_PERCENT;

    int span_temp    = FAN_TEMP_HIGH_C - FAN_TEMP_LOW_C;   // e.g. 35
    int span_percent = FAN_MAX_PERCENT - FAN_MIN_PERCENT;  // e.g. 90
    int delta        = temp_c - FAN_TEMP_LOW_C;

    return FAN_MIN_PERCENT + (unsigned)(delta * span_percent) / span_temp;
}

// STUB: this is where the real PWM duty-cycle write goes once RP1's
// PWM peripheral is reachable. For now it just reports intent.
static void fan_apply(unsigned percent) {
    uart_puts("[fan] would set duty to ");
    uart_put_hex64(percent);
    uart_puts("% (RP1 PWM not wired up yet - no physical effect)\n");
}

// Call this periodically (e.g. once per heartbeat tick) once a timer
// driver exists; for now it can be called inline from kmain's loop.
void fan_update(void) {
    uint32_t millic = mbox_get_temp_millic();
    if (millic == 0xFFFFFFFF) {
        uart_puts("[fan] temperature read failed, holding floor speed\n");
        fan_apply(FAN_MIN_PERCENT);
        return;
    }
    int temp_c = (int)(millic / 1000);
    unsigned percent = fan_curve_percent(temp_c);
    fan_apply(percent);
}
