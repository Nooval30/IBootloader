#ifndef BOOTINFO_H
#define BOOTINFO_H
#include "boot_info.h"

/* copy of what the bootloader handed us (defined in kmain.c) */
extern boot_info_t g_boot_info;
extern int g_boot_valid;

#endif
