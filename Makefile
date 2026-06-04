CC  = gcc
CXX = g++
LD  = ld
AS  = nasm

CXXFLAGS = -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector -fno-pic -fno-pie -fno-plt -fno-exceptions -fno-rtti -std=c++17 -O2

.PHONY: all clean run

all: GitFreedom.img

kernel.o: kernel.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

kernel.bin: kernel.o link.ld
	$(LD) -m elf_i386 -T link.ld -o $@ kernel.o --oformat binary -nostdlib

boot.bin: boot.asm kernel.bin
	$(AS) -f bin boot.asm -o $@

GitFreedom.img: boot.bin
	dd if=/dev/zero bs=1M count=10 of=$@ 2>/dev/null
	dd if=boot.bin of=$@ conv=notrunc 2>/dev/null
	@echo "=== GitFreedom OS Compiled Successfully ==="

run: GitFreedom.img
	qemu-system-i386 -hda GitFreedom.img -m 16M

clean:
	rm -f *.o *.bin *.img
