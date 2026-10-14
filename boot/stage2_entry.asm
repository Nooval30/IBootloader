; ---------------------------------------------------------------------------
; Stage 2 entry (16-bit real mode -> 32-bit protected mode), then calls C.
;  1. Save boot drive
;  2. Collect BIOS E820 memory map at 0x500 (count) / 0x510 (entries)
;  3. Enable A20, load GDT, switch to protected mode
;  4. call stage2_main (C)
; ---------------------------------------------------------------------------
global _start
global boot_drive
extern stage2_main

section .entry progbits alloc exec write align=4

BITS 16
_start:
    cli
    xor  ax, ax
    mov  ds, ax
    mov  es, ax
    mov  ss, ax
    mov  sp, 0x7C00
    mov  [boot_drive], dl

    ; ---- E820 memory map ----
    mov  di, 0x510
    xor  ebx, ebx
    xor  bp, bp
.e820_loop:
    mov  dword [es:di + 20], 1
    mov  eax, 0xE820
    mov  edx, 0x534D4150        ; 'SMAP'
    mov  ecx, 24
    int  0x15
    jc   .e820_done
    cmp  eax, 0x534D4150
    jne  .e820_done
    inc  bp
    add  di, 24
    test ebx, ebx
    jz   .e820_done
    cmp  bp, 32
    jb   .e820_loop
.e820_done:
    mov  [0x500], bp

    ; ---- A20 (fast gate) ----
    in   al, 0x92
    or   al, 2
    and  al, 0xFE
    out  0x92, al

    ; ---- protected mode ----
    cli
    a32 lgdt [gdt_desc]
    mov  eax, cr0
    or   eax, 1
    mov  cr0, eax
    jmp  dword 0x08:pm_entry

BITS 32
pm_entry:
    mov  ax, 0x10
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax
    mov  esp, 0x90000
    cld
    call stage2_main
.hang:
    cli
    hlt
    jmp  .hang

align 8
gdt:
    dq 0x0000000000000000       ; null
    dq 0x00CF9A000000FFFF       ; 0x08: 32-bit code, base 0, limit 4G
    dq 0x00CF92000000FFFF       ; 0x10: 32-bit data, base 0, limit 4G
gdt_end:
gdt_desc:
    dw gdt_end - gdt - 1
    dd gdt

boot_drive: dd 0
