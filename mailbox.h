// mailbox.h - ARM<->VideoCore mailbox property interface.
#ifndef MAILBOX_H
#define MAILBOX_H

#include <stdint.h>

// Sends a property-interface message buffer (must be 16-byte aligned,
// first word = total size in bytes, last word = 0 end tag) and waits
// for the firmware's response. Returns 1 on success, 0 on failure.
int mbox_call(volatile uint32_t* buf);

// Reads CPU temperature via the VideoCore firmware mailbox (tag 0x00030006).
// Works today - no RP1/PCIe dependency, unlike fan actuation.
// Returns millidegrees Celsius, or 0xFFFFFFFF on failure.
uint32_t mbox_get_temp_millic(void);

#endif
