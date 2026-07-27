# Nex OS — Stage 1 Bootstrap for Raspberry Pi 5

This is a from-scratch bare-metal boot stage for the Pi 5 (BCM2712,
Cortex-A76, AArch64). It boots the CPU, parks 3 of the 4 cores, sets up
a stack, zeroes .bss, and prints a heartbeat message over a dedicated
debug UART — the standard "did the machine actually boot my code"
milestone in OS development.

**This has been cross-compiled and structurally verified in this session
(correct ELF entry point, correct AArch64 instructions, correct linker
layout) but has NOT been tested on physical Pi 5 hardware** — I don't have
a Pi 5 to test on. Read the risk section before assuming it'll work first try.

## What's genuinely solid here

- **Boot partition requirements** (`config.txt` with `os_check=0`,
  `arm_64bit=1`, `kernel=kernel8.img`) — corroborated by multiple current
  Raspberry Pi forum threads and the official `config.txt` documentation.
- **Load address 0x80000** — this is the documented address the Pi 5
  firmware uses once `os_check=0` tells it "not Linux, don't require a
  matching device tree." Confirmed by a Pi 5 forum thread specifically
  about bare-metal boot.
- **Core-parking / BSS-zeroing / linker script** — this is standard AArch64
  bring-up code, unchanged from Pi 3/4 bare-metal tutorials, and I verified
  the actual generated machine code with objdump (shown below).

## The one part I could not verify — please read this

On the Pi 5, most I/O (GPIO, the classic pins-14/15 UART) moved off the
main SoC onto a separate "RP1" chip connected over PCIe. Reading RP1's
registers **before PCIe is brought up causes a hard crash (data abort)** —
so all the classic Pi 3/4 bare-metal UART tutorials do not directly apply.

Instead, this code targets the Pi 5's **separate, dedicated debug UART**
(exposed on a small header near the USB-C power port, not the GPIO pins),
at physical address `0x107D001000`. I found exactly **one** corroborating
source for this address (a Raspberry Pi forum reply giving
`earlycon=pl011,0x107d001000` for that UART) and could not cross-reference
it against an official ARM/Broadcom register map in this session.

**If you boot this and see nothing on the debug UART:** the boot chain
past this point may still be totally fine — it's specifically this address
(and/or the reference clock assumption used for the baud-rate divisor in
`uart.c`) that's the weak link, not the entry code or linker layout.

### If the debug UART doesn't work, two paths forward:
1. **Easiest first troubleshooting step:** connect a USB-to-TTL serial
   adapter to the dedicated debug UART header (check the official Pi 5
   product brief for its exact pin diagram — it's a 3-pin header, not the
   40-pin GPIO header), 115200 8N1, and see if you get the boot banner.
2. **If that address is wrong:** the alternative is enabling
   `pciex4_reset=0` in config.txt (keeps the RP1 PCIe link that firmware
   otherwise resets before handoff) and writing a minimal PCIe root-complex
   + RP1 register driver to reach the header UART — meaningfully more work,
   and something I deliberately didn't attempt blind, since guessing at
   more unverified addresses on top of an already-uncertain one compounds
   the risk rather than reducing it.

## SD card setup

1. Format an SD card's first partition as FAT32 (or use one already set
   up by Raspberry Pi Imager with any OS — the Pi 5's own firmware files
   are already there; **don't delete them**).
2. Copy this project's `config.txt` and the built `kernel8.img` into the
   root of that FAT32 partition, alongside the existing firmware files.
3. Insert the SD card into the Pi 5, connect the debug UART header to a
   USB-to-TTL adapter, open a serial terminal at 115200 8N1, power on.

## Building

Requires an AArch64 cross-compiler (`gcc-aarch64-linux-gnu` on Debian/Ubuntu):

```bash
make
```

Produces `kernel8.img` — the raw binary that goes on the SD card.

## Verifying the build (already done in this session)

```
$ aarch64-linux-gnu-readelf -h kernel.elf | grep -E "Entry|Machine"
  Class:    ELF64
  Machine:  AArch64
  Entry point address: 0x80000
```

Disassembly confirms: MPIDR core-ID check → non-zero cores parked in
`wfe` loop → stack pointer set → .bss zeroed → branch to `kmain`. All
matches the intended design.

## Sources consulted

- Raspberry Pi official `config.txt` docs (`os_check`, `arm_64bit`, `kernel`)
- Raspberry Pi Forums: "Pi5 Boot Process," "UART on RPi5," "Network boot pi5"
- ARM Trusted Firmware-A docs for rpi5 (armstub/BL31 boot stages)
- Community bare-metal writeup: main.lv/writeup/raspberry5_baremetal_uart.md
  (source of the 0x107d001000 debug-UART address — the unverified part)

## Next steps if this boots successfully

- Add a hardware timer driver (ARM generic timer, `CNTP_*` registers —
  standard across all AArch64 boards, not Pi-specific, so lower risk)
- Add an exception vector table so crashes give a diagnostic instead of
  silently hanging
- Only after UART + timers are confirmed working: attempt the RP1/PCIe
  bring-up needed for GPIO, the header UART, and eventually storage/network
  drivers for the file-server OS described earlier in this conversation
