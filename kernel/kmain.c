/* Kernel entry point: bring up each subsystem in order, then run the shell. */
#include "bootinfo.h"
#include "console.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"
#include "pmm.h"
#include "heap.h"
#include "shell.h"

boot_info_t g_boot_info;
int g_boot_valid;

/* kernel.ld places this section first, right after the 4-byte magic (entry = KERNEL_BASE + 4) */
__attribute__((section(".text.kmain")))
void kmain(boot_info_t *bi) {
    if (bi && bi->magic == BOOTINFO_MAGIC) { g_boot_info = *bi; g_boot_valid = 1; }

    gdt_init();
    idt_init();
    pic_init();
    timer_init(100);
    keyboard_init();

    if (g_boot_valid && pmm_init((e820_entry_t *)g_boot_info.mmap_addr, g_boot_info.mmap_count)) {
        uint32_t hp = pmm_alloc_contig(HEAP_FRAMES);
        if (hp) heap_init((void *)hp, HEAP_FRAMES * FRAME_SIZE);
    }

    kset_color(COLOR_GREEN);
    kputs("\n>>> Kernel up: GDT, IDT, timer, keyboard, memory. Type 'help'. <<<\n");
    kset_color(COLOR_GRAY);

    shell_run();
}
