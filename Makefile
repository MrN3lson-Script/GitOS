AS = nasm
CC = i686-elf-g++
LD = i686-elf-gcc

ASFLAGS = -f elf32
CFLAGS = -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti
LDFLAGS = -T linker.ld -ffreestanding -O2 -nostdlib

OBJS = boot.o kernel.o

gitos.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^ -lgcc

boot.o: boot.asm
	$(AS) $(ASFLAGS) $< -o $@

kernel.o: kernel.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o gitos.bin

run: gitos.bin
	qemu-system-i386 -kernel gitos.bin
