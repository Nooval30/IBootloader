#ifndef BOOT_INFO_H
#define BOOT_INFO_H
#include <stdint.h>

#define KERNEL_MAGIC   0x4C4E524Bu   /* "KRNL" little-endian */
#define KERNEL_BASE    0x10000u
#define BOOTINFO_MAGIC 0xB007CAFEu

/* BIOS INT 15h/E820 entry */
typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;      /* 1 = usable, 2 = reserved, 3 = ACPI reclaimable ... */
    uint32_t acpi;
} __attribute__((packed)) e820_entry_t;

/* Handed to the kernel (cdecl, first argument) */
typedef struct {
    uint32_t magic;
    uint32_t boot_drive;
    uint32_t mmap_count;
    uint32_t mmap_addr;      /* physical address of e820_entry_t[] */
    uint32_t usable_kb;      /* total usable RAM in KiB */
} __attribute__((packed)) boot_info_t;

#endif
