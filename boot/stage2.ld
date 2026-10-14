OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)
SECTIONS
{
    . = 0x7E00;
    .text   : { *(.entry) *(.text*) }
    .rodata : { *(.rodata*) }
    /* .bss is merged into .data so it is part of the flat binary (zeroed) */
    .data   : { *(.data*) *(.bss*) *(COMMON) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
