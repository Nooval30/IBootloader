/* Mini kernel: GDT + IDT + PIC + PIT timer + keyboard + tiny shell.
 * Loaded by the bootloader at 0x10000 (entry kmain at +4). */
#include <stdint.h>
#include "boot_info.h"

#define VGA  ((volatile uint16_t *)0xB8000)
#define COM1 0x3F8

static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1" :: "a"(v), "Nd"(p)); }
static inline uint8_t inb(uint16_t p) { uint8_t v; __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p)); return v; }

/* ============================ console ============================ */
static int cx = 0, cy = 14;          /* start below the bootloader's output */
static uint8_t col = 0x07;

static void cursor_update(void) {
    uint16_t pos = (uint16_t)(cy * 80 + cx);
    outb(0x3D4, 0x0F); outb(0x3D5, pos & 0xFF);
    outb(0x3D4, 0x0E); outb(0x3D5, pos >> 8);
}
static void ser(char c) { while (!(inb(COM1 + 5) & 0x20)) {} outb(COM1, (uint8_t)c); }

static void scroll(void) {
    for (int i = 0; i < 80 * 24; i++) VGA[i] = VGA[i + 80];
    for (int i = 0; i < 80; i++) VGA[80 * 24 + i] = (uint16_t)(col << 8) | ' ';
    cy = 24;
}
static void putc(char c) {
    if (c == '\n') { ser('\r'); ser('\n'); cx = 0; cy++; }
    else if (c == '\b') {
        if (cx > 0) { cx--; VGA[cy * 80 + cx] = (uint16_t)(col << 8) | ' '; ser('\b'); ser(' '); ser('\b'); }
    } else {
        ser(c);
        VGA[cy * 80 + cx] = (uint16_t)(col << 8) | (uint8_t)c;
        if (++cx >= 80) { cx = 0; cy++; }
    }
    if (cy >= 25) scroll();
    cursor_update();
}
static void puts(const char *s) { while (*s) putc(*s++); }
static void clear(void) {
    for (int i = 0; i < 80 * 25; i++) VGA[i] = (uint16_t)(0x07 << 8) | ' ';
    cx = cy = 0; cursor_update();
}
static void dec(uint32_t v) {
    char b[11]; int i = 0;
    if (!v) { putc('0'); return; }
    while (v) { b[i++] = (char)('0' + v % 10); v /= 10; }
    while (i) putc(b[--i]);
}
static void hex(uint64_t v, int digits) {
    static const char h[] = "0123456789ABCDEF";
    puts("0x");
    for (int i = (digits - 1) * 4; i >= 0; i -= 4) putc(h[(v >> i) & 0xF]);
}

/* ============================ GDT ============================ */
struct gdt_entry { uint16_t limit_lo, base_lo; uint8_t base_mid, access, gran, base_hi; } __attribute__((packed));
struct dtr { uint16_t limit; uint32_t base; } __attribute__((packed));
static struct gdt_entry gdt[3];

static void gdt_init(void) {
    gdt[1] = (struct gdt_entry){ 0xFFFF, 0, 0, 0x9A, 0xCF, 0 };   /* 0x08 code */
    gdt[2] = (struct gdt_entry){ 0xFFFF, 0, 0, 0x92, 0xCF, 0 };   /* 0x10 data */
    struct dtr r = { sizeof(gdt) - 1, (uint32_t)gdt };
    __asm__ volatile(
        "lgdt %0\n"
        "ljmp $0x08, $1f\n"
        "1: mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n mov %%ax, %%es\n mov %%ax, %%fs\n mov %%ax, %%gs\n mov %%ax, %%ss\n"
        :: "m"(r) : "ax", "memory");
}

/* ============================ IDT + exceptions ============================ */
struct idt_entry { uint16_t off_lo, sel; uint8_t zero, flags; uint16_t off_hi; } __attribute__((packed));
struct int_frame { uint32_t eip, cs, eflags; };
static struct idt_entry idt[256];

static void idt_set(int n, void (*h)(void)) {
    uint32_t a = (uint32_t)h;
    idt[n] = (struct idt_entry){ a & 0xFFFF, 0x08, 0, 0x8E, a >> 16 };
}

static void exc_panic(int n) {
    col = 0x4F;
    puts("\nCPU EXCEPTION #"); dec((uint32_t)n); puts(" - halted\n");
    for (;;) __asm__ volatile("cli; hlt");
}
#define EXC(n)     static void __attribute__((interrupt)) exc##n(struct int_frame *f) { (void)f; exc_panic(n); }
#define EXC_ERR(n) static void __attribute__((interrupt)) exc##n(struct int_frame *f, uint32_t e) { (void)f; (void)e; exc_panic(n); }
EXC(0) EXC(1) EXC(2) EXC(3) EXC(4) EXC(5) EXC(6) EXC(7) EXC_ERR(8) EXC(9)
EXC_ERR(10) EXC_ERR(11) EXC_ERR(12) EXC_ERR(13) EXC_ERR(14) EXC(15) EXC(16) EXC_ERR(17) EXC(18) EXC(19)
EXC(20) EXC_ERR(21) EXC(22) EXC(23) EXC(24) EXC(25) EXC(26) EXC(27) EXC(28) EXC_ERR(29) EXC_ERR(30) EXC(31)
#define E(n) (void (*)(void))exc##n
static void (*const exc_table[32])(void) = {
    E(0),E(1),E(2),E(3),E(4),E(5),E(6),E(7),E(8),E(9),E(10),E(11),E(12),E(13),E(14),E(15),
    E(16),E(17),E(18),E(19),E(20),E(21),E(22),E(23),E(24),E(25),E(26),E(27),E(28),E(29),E(30),E(31)
};

/* ============================ PIC + PIT ============================ */
static volatile uint32_t ticks;

static void pic_init(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11);     /* init */
    outb(0x21, 0x20); outb(0xA1, 0x28);     /* remap IRQs to 0x20.. / 0x28.. */
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xFC);                       /* unmask IRQ0 (timer) + IRQ1 (keyboard) */
    outb(0xA1, 0xFF);
}
static void pit_init(void) {                /* 100 Hz */
    uint16_t d = 11931;
    outb(0x43, 0x36); outb(0x40, d & 0xFF); outb(0x40, d >> 8);
}
static void __attribute__((interrupt)) irq0(struct int_frame *f) { (void)f; ticks++; outb(0x20, 0x20); }

/* ============================ keyboard ============================ */
static const char km_lo[] = "\0\0" "1234567890-=" "\b\t" "qwertyuiop[]" "\n\0" "asdfghjkl;'" "`\0" "\\" "zxcvbnm,./" "\0*\0 ";
static const char km_up[] = "\0\0" "!@#$%^&*()_+" "\b\t" "QWERTYUIOP{}" "\n\0" "ASDFGHJKL:\"" "~\0" "|" "ZXCVBNM<>?" "\0*\0 ";

static volatile uint8_t kbuf[256];
static volatile uint8_t khead, ktail;
static int shift, caps, ext;

static void kb_push(char c) {
    uint8_t n = (uint8_t)(khead + 1);
    if (n != ktail) { kbuf[khead] = (uint8_t)c; khead = n; }
}
static void __attribute__((interrupt)) irq1(struct int_frame *f) {
    (void)f;
    uint8_t sc = inb(0x60);
    if (sc == 0xE0) ext = 1;
    else if (ext) ext = 0;                              /* ignore arrows etc. */
    else if (sc == 0x2A || sc == 0x36) shift = 1;
    else if (sc == 0xAA || sc == 0xB6) shift = 0;
    else if (sc == 0x3A) caps ^= 1;
    else if (!(sc & 0x80) && sc < sizeof(km_lo) - 1) {
        char c = shift ? km_up[sc] : km_lo[sc];
        if (caps && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) c ^= 0x20;
        if (c && c != '\t') kb_push(c);
    }
    outb(0x20, 0x20);
}

/* ============================ shell ============================ */
static boot_info_t boot;
static int have_boot;
static char line[80];
static int len;

static int streq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
static int starts(const char *s, const char *p) { while (*p) if (*s++ != *p++) return 0; return 1; }

static void prompt(void) { col = 0x0B; puts("kernel> "); col = 0x07; }

static void cmd_memmap(void) {
    if (!have_boot) { puts("no boot info\n"); return; }
    e820_entry_t *e = (e820_entry_t *)boot.mmap_addr;
    for (uint32_t i = 0; i < boot.mmap_count; i++) {
        hex(e[i].base, 9); puts(" - "); hex(e[i].base + e[i].length, 9);
        puts(e[i].type == 1 ? "  usable\n" : "  reserved\n");
    }
}

static void run(char *c) {
    while (*c == ' ') c++;
    if (!*c) return;
    if (streq(c, "help")) {
        puts("commands:\n"
             "  help      this list\n"
             "  clear     clear the screen\n"
             "  mem       usable RAM\n"
             "  memmap    BIOS memory map\n"
             "  uptime    time since boot\n"
             "  echo TXT  print TXT\n"
             "  reboot    restart the machine\n"
             "  halt      stop the CPU\n");
    } else if (streq(c, "clear")) clear();
    else if (streq(c, "mem")) {
        if (have_boot) { dec(boot.usable_kb / 1024); puts(" MiB usable ("); dec(boot.usable_kb); puts(" KiB)\n"); }
        else puts("no boot info\n");
    } else if (streq(c, "memmap")) cmd_memmap();
    else if (streq(c, "uptime")) { dec(ticks / 100); puts(" s ("); dec(ticks); puts(" ticks)\n"); }
    else if (starts(c, "echo ")) { puts(c + 5); putc('\n'); }
    else if (streq(c, "echo")) putc('\n');
    else if (streq(c, "reboot")) {
        while (inb(0x64) & 0x02) {}
        outb(0x64, 0xFE);
        for (;;) __asm__ volatile("hlt");
    } else if (streq(c, "halt")) {
        puts("halted.\n");
        for (;;) __asm__ volatile("cli; hlt");
    } else { col = 0x0C; puts("unknown command: "); puts(c); putc('\n'); col = 0x07; }
}

static void handle_char(char c) {
    if (c == '\n') { putc('\n'); line[len] = 0; run(line); len = 0; prompt(); }
    else if (c == '\b') { if (len > 0) { len--; putc('\b'); } }
    else if (len < (int)sizeof(line) - 1) { line[len++] = c; putc(c); }
}

/* ============================ entry ============================ */
__attribute__((section(".text.kmain")))
void kmain(boot_info_t *bi) {
    if (bi && bi->magic == BOOTINFO_MAGIC) { boot = *bi; have_boot = 1; }

    gdt_init();
    for (int i = 0; i < 32; i++) idt_set(i, exc_table[i]);
    idt_set(0x20, (void (*)(void))irq0);
    idt_set(0x21, (void (*)(void))irq1);
    struct dtr r = { sizeof(idt) - 1, (uint32_t)idt };
    __asm__ volatile("lidt %0" :: "m"(r));
    pic_init();
    pit_init();

    col = 0x0A; puts("\n>>> Kernel up: GDT, IDT, timer, keyboard. Type 'help'. <<<\n"); col = 0x07;
    prompt();
    __asm__ volatile("sti");

    for (;;) {
        __asm__ volatile("cli");
        if (khead == ktail) { __asm__ volatile("sti; hlt"); continue; }
        char c = (char)kbuf[ktail]; ktail = (uint8_t)(ktail + 1);
        __asm__ volatile("sti");
        handle_char(c);
    }
}
