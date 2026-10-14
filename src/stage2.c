/* Stage 2 bootloader, written in C (32-bit protected mode, freestanding). */
#include <stdint.h>
#include "boot_info.h"

extern uint32_t boot_drive;               /* set by stage2_entry.asm */

#define VGA_MEM   ((volatile uint16_t *)0xB8000)
#define VGA_W     80
#define VGA_H     25
#define COM1      0x3F8
#define E820_COUNT (*(volatile uint16_t *)0x500)
#define E820_TABLE ((e820_entry_t *)0x510)

static int cx = 0, cy = 0;
static uint8_t color = 0x07;
static boot_info_t info;

static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1" :: "a"(v), "Nd"(p)); }
static inline uint8_t inb(uint16_t p) { uint8_t v; __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p)); return v; }

static void serial_init(void) {
    outb(COM1 + 1, 0x00); outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03); outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03); outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}
static void serial_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20)) {}
    outb(COM1, (uint8_t)c);
}

static void vga_clear(void) {
    for (int i = 0; i < VGA_W * VGA_H; i++) VGA_MEM[i] = (uint16_t)(color << 8) | ' ';
    cx = cy = 0;
}
static void vga_scroll(void) {
    for (int i = 0; i < VGA_W * (VGA_H - 1); i++) VGA_MEM[i] = VGA_MEM[i + VGA_W];
    for (int i = 0; i < VGA_W; i++) VGA_MEM[VGA_W * (VGA_H - 1) + i] = (uint16_t)(color << 8) | ' ';
    cy = VGA_H - 1;
}

static void putc(char c) {
    if (c == '\n') { serial_putc('\r'); serial_putc('\n'); cx = 0; cy++; }
    else {
        serial_putc(c);
        VGA_MEM[cy * VGA_W + cx] = (uint16_t)(color << 8) | (uint8_t)c;
        if (++cx >= VGA_W) { cx = 0; cy++; }
    }
    if (cy >= VGA_H) vga_scroll();
}
static void puts(const char *s) { while (*s) putc(*s++); }

static void put_hex(uint64_t v, int digits) {
    static const char hex[] = "0123456789ABCDEF";
    puts("0x");
    for (int i = (digits - 1) * 4; i >= 0; i -= 4) putc(hex[(v >> i) & 0xF]);
}
static void put_dec(uint32_t v) {
    char buf[11]; int i = 0;
    if (!v) { putc('0'); return; }
    while (v) { buf[i++] = '0' + v % 10; v /= 10; }
    while (i) putc(buf[--i]);
}

static const char *e820_name(uint32_t t) {
    switch (t) {
        case 1: return "usable";
        case 2: return "reserved";
        case 3: return "ACPI reclaim";
        case 4: return "ACPI NVS";
        case 5: return "bad";
        default: return "unknown";
    }
}

static void cpu_vendor(char out[13]) {
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0));
    *(uint32_t *)(out + 0) = b;
    *(uint32_t *)(out + 4) = d;
    *(uint32_t *)(out + 8) = c;
    out[12] = 0;
}

static void panic(const char *msg) {
    color = 0x4F;
    puts("\nPANIC: "); puts(msg);
    for (;;) __asm__ volatile("cli; hlt");
}

void stage2_main(void) {
    serial_init();
    color = 0x07;
    vga_clear();

    color = 0x0B;
    puts("=== C Bootloader (stage 2) ===\n");
    color = 0x07;

    char vendor[13];
    cpu_vendor(vendor);
    puts("CPU vendor : "); puts(vendor); putc('\n');
    puts("Boot drive : "); put_hex(boot_drive & 0xFF, 2); putc('\n');

    uint32_t n = E820_COUNT;
    uint64_t usable = 0;
    puts("Memory map ("); put_dec(n); puts(" entries):\n");
    for (uint32_t i = 0; i < n; i++) {
        e820_entry_t *e = &E820_TABLE[i];
        puts("  "); put_hex(e->base, 9); puts(" - ");
        put_hex(e->base + e->length, 9); puts("  ");
        puts(e820_name(e->type)); putc('\n');
        if (e->type == 1) usable += e->length;
    }
    puts("Usable RAM : "); put_dec((uint32_t)(usable >> 10)); puts(" KiB\n");

    info.magic      = BOOTINFO_MAGIC;
    info.boot_drive = boot_drive & 0xFF;
    info.mmap_count = n;
    info.mmap_addr  = 0x510;
    info.usable_kb  = (uint32_t)(usable >> 10);

    if (*(volatile uint32_t *)KERNEL_BASE != KERNEL_MAGIC)
        panic("kernel image not found / bad magic");

    puts("Kernel OK, jumping to "); put_hex(KERNEL_BASE + 4, 8); putc('\n');

    void (*kernel_entry)(boot_info_t *) = (void (*)(boot_info_t *))(KERNEL_BASE + 4);
    kernel_entry(&info);
    panic("kernel returned");
}
