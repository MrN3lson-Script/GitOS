[BITS 16]
[ORG 0x7C00]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [BOOT_DRIVE], dl

    mov ah, 0x42
    mov dl, [BOOT_DRIVE]
    mov si, dap
    int 0x13
    jc .disk_error

    cli
    lgdt [gdt_desc]
    mov eax, cr0
    or  eax, 1
    mov cr0, eax
    jmp 0x08:pm32

.disk_error:
    mov ax, 0xB800
    mov es, ax
    mov word [es:0], 0x0F45
.hang:
    jmp .hang

align 4
dap:
    db 0x10, 0
    dw 64
    dw 0x7E00, 0x0000
    dq 1

[BITS 32]
pm32:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov esp, 0x90000
    jmp 0x7E00

BOOT_DRIVE db 0

align 8
gdt_start:
    dq 0
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:
gdt_desc:
    dw gdt_end - gdt_start - 1
    dd gdt_start

times 510-($-$$) db 0
dw 0xAA55

kernel_data:
incbin "kernel.bin"
