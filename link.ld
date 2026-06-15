ENTRY(kmain)
OUTPUT_FORMAT(binary)
SECTIONS {
    . = 0x7E00;
    .text   : { 
        *(.text.boot)
        *(.text*) 
    }
    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }
    .bss    : { 
        *(.bss*) 
        *(COMMON) 
    }
    . = ALIGN(4);
    _end = .;
}
