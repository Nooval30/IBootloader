# C Bootloader (x86, BIOS, 2-stage)

Bootloader kecil buat PC BIOS/legacy. Logika utamanya ditulis di **C**; assembly cuma dipakai
buat hal yang emang nggak bisa di C (MBR 512 byte + switch ke protected mode).

## Alur boot
1. **stage1.asm** (MBR, 512 byte): load stage 2 (LBA 1, 16 sektor) ke `0x7E00` dan kernel (32 sektor) ke `0x10000` pakai BIOS INT 13h extended read.
2. **stage2_entry.asm**: ambil memory map BIOS (E820), enable A20, load GDT, masuk 32-bit protected mode, `call stage2_main`.
3. **src/stage2.c** (C): init VGA + serial, tampilin CPU vendor, boot drive, memory map, cek magic kernel, lalu lompat ke kernel sambil kirim `boot_info_t`.
4. **kernel/kernel.c**: kernel demo, print "Hello from the kernel" + info RAM.

## Layout disk (`build/disk.img`)
| Sektor | Isi |
|---|---|
| 0 | stage1 (MBR) |
| 1-16 | stage2 |
| 17-48 | kernel |

## Build & run (Linux / WSL / Ubuntu)
```
sudo apt install nasm gcc binutils make qemu-system-x86
make          # hasil: build/disk.img
make run      # jalanin di QEMU (window)
make run-nogui   # QEMU tanpa window, output lewat serial di terminal
```
Di Windows paling gampang pakai **WSL2** (Ubuntu), terus install QEMU di Windows atau pakai `make run-nogui` di dalam WSL.

## Tulis ke USB (HATI-HATI, ini ngehapus isi device)
```
sudo dd if=build/disk.img of=/dev/sdX bs=512 conv=notrunc
```
Ganti `/dev/sdX` dengan device yang bener (cek `lsblk`). Cuma jalan di BIOS/CSM mode, bukan UEFI murni.

## Struktur
```
boot/stage1.asm        MBR
boot/stage2_entry.asm  real mode -> protected mode
boot/stage2.ld         linker script stage 2 (base 0x7E00)
src/stage2.c           bootloader utama (C)
kernel/kernel.c        kernel demo (base 0x10000)
kernel/kernel.ld
include/boot_info.h    struct yang dishare bootloader <-> kernel
Makefile
```
