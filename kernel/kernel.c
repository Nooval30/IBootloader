/* Tiny demo kernel loaded by the bootloader at 0x10000. */
#include <stdint.h>
#include "boot_info.h"

#define VGA ((volatile uint16_t *)0xB8000)
#define COM1 0x3F8
static int pos = 0;

static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1" :: "a"(v), "Nd"(p)); }
static inline uint8_t inb(uint16_t p) { uint8_t v; __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p)); return v; }

static void ser(char c) {
    while (!(inb(COM1 + 5) & 0x20)) {}
    outb(COM1, (uint8_t)c);
}

static void out(const char *s, uint8_t col) {
    while (*s) {
        if (*s == '\n') { pos = (pos / 80 + 1) * 80; ser('\r'); ser('\n'); }
        else { ser(*s); VGA[pos++] = (uint16_t)(col << 8) | (uint8_t)*s; }
        s++;
    }
}
static void dec(uint32_t v, uint8_t col) {
    char b[12]; int i = 11; b[i] = 0;
    if (!v) b[--i] = '0';
    while (v) { b[--i] = '0' + v % 10; v /= 10; }
    out(&b[i], col);
}

__attribute__((section(".text.kmain")))
void kmain(boot_info_t *bi) {
    pos = 80 * 14;                      /* print below the bootloader output */
    out("\n>>> Hello from the kernel, loaded by the C bootloader! <<<\n", 0x0A);
    if (bi && bi->magic == BOOTINFO_MAGIC) {
        out("boot_info OK: usable RAM = ", 0x07); dec(bi->usable_kb / 1024, 0x07);
        out(" MiB, e820 entries = ", 0x07); dec(bi->mmap_count, 0x07); out("\n", 0x07);
    }
    out("Halting.\n", 0x07);
    for (;;) __asm__ volatile("cli; hlt");
}
