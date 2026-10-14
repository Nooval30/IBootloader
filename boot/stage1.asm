; ---------------------------------------------------------------------------
; Stage 1: 512-byte MBR boot sector.
; Uses BIOS INT 13h extended read to load:
;   - stage 2 (STAGE2_SECTORS) from LBA 1       -> 0000:7E00
;   - kernel  (KERNEL_SECTORS) after stage 2    -> 1000:0000 (0x10000)
; then jumps to stage 2 with DL = boot drive.
; Build with: nasm -f bin -DSTAGE2_SECTORS=16 -DKERNEL_SECTORS=32
; ---------------------------------------------------------------------------
BITS 16
ORG 0x7C00

start:
    cli
    xor  ax, ax
    mov  ds, ax
    mov  es, ax
    mov  ss, ax
    mov  sp, 0x7C00
    sti
    cld
    mov  [boot_drive], dl

    mov  si, msg_boot
    call print

    ; ---- load stage 2 ----
    mov  word  [dap_count], STAGE2_SECTORS
    mov  word  [dap_off],   0x7E00
    mov  word  [dap_seg],   0x0000
    mov  dword [dap_lba],   1
    call read_disk

    ; ---- load kernel ----
    mov  word  [dap_count], KERNEL_SECTORS
    mov  word  [dap_off],   0x0000
    mov  word  [dap_seg],   0x1000
    mov  dword [dap_lba],   1 + STAGE2_SECTORS
    call read_disk

    mov  dl, [boot_drive]
    jmp  0x0000:0x7E00

; ---------------------------------------------------------------------------
read_disk:
    mov  ah, 0x42
    mov  dl, [boot_drive]
    mov  si, dap
    int  0x13
    jc   disk_error
    ret

disk_error:
    mov  si, msg_err
    call print
.hang:
    cli
    hlt
    jmp  .hang

print:                      ; DS:SI = zero-terminated string
    mov  ah, 0x0E
    xor  bx, bx
.next:
    lodsb
    test al, al
    jz   .done
    int  0x10
    jmp  .next
.done:
    ret

; ---------------------------------------------------------------------------
boot_drive: db 0
msg_boot:   db "Stage1: loading...", 13, 10, 0
msg_err:    db "Disk read error!", 13, 10, 0

align 4
dap:                        ; Disk Address Packet
    db 0x10, 0
dap_count: dw 0
dap_off:   dw 0
dap_seg:   dw 0
dap_lba:   dq 0

times 510 - ($ - $$) db 0
dw 0xAA55
