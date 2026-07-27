CROSS   = aarch64-linux-gnu-
CC      = $(CROSS)gcc
LD      = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy

CFLAGS  = -ffreestanding -nostdlib -nostartfiles -fno-stack-protector \
          -mgeneral-regs-only -O2 -Wall -Wextra -c
ASFLAGS = -c

all: kernel8.img

boot.o: boot.S
	$(CC) $(ASFLAGS) boot.S -o boot.o

uart.o: uart.c
	$(CC) $(CFLAGS) uart.c -o uart.o

kmain.o: kmain.c
	$(CC) $(CFLAGS) kmain.c -o kmain.o

kernel.elf: boot.o uart.o kmain.o linker.ld
	$(LD) -T linker.ld -o kernel.elf boot.o uart.o kmain.o

kernel8.img: kernel.elf
	$(OBJCOPY) -O binary kernel.elf kernel8.img

clean:
	rm -f *.o kernel.elf kernel8.img

.PHONY: all clean
