OUTPUT_FORMAT(elf32-i386)
ENTRY(kmain)
SECTIONS
{
    . = 0x10000;
    .header : { LONG(0x4C4E524B) }          /* magic "KRNL", entry follows at +4 */
    .text   : { *(.text.kmain) *(.text*) }
    .rodata : { *(.rodata*) }
    .data   : { *(.data*) *(.bss*) *(COMMON) }
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
