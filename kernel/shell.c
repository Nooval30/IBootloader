/* Tiny command-line shell. To add a command: write cmd_xxx() and add a line to cmds[]. */
#include "shell.h"
#include "console.h"
#include "keyboard.h"
#include "timer.h"
#include "pmm.h"
#include "heap.h"
#include "kstring.h"
#include "bootinfo.h"
#include "io.h"

typedef void (*cmd_fn)(char *args);
struct cmd { const char *name, *help; cmd_fn fn; };

static void *slots[16];                 /* allocations made with the 'alloc' command */

/* ---------------- commands ---------------- */
static void cmd_help(char *a);

static void cmd_clear(char *a) { (void)a; kclear(); }

static void cmd_echo(char *a) { kputs(a); kputc('\n'); }

static void cmd_uptime(char *a) {
    (void)a;
    uint32_t t = timer_ticks();
    kdec(t / timer_hz()); kputs(" s ("); kdec(t); kputs(" ticks)\n");
}

static void cmd_mem(char *a) {
    (void)a;
    if (!g_boot_valid) { kputs("no boot info\n"); return; }
    kdec(g_boot_info.usable_kb / 1024); kputs(" MiB usable ("); kdec(g_boot_info.usable_kb); kputs(" KiB)\n");
}

static void cmd_memmap(char *a) {
    (void)a;
    if (!g_boot_valid) { kputs("no boot info\n"); return; }
    e820_entry_t *e = (e820_entry_t *)g_boot_info.mmap_addr;
    for (uint32_t i = 0; i < g_boot_info.mmap_count; i++) {
        khex(e[i].base, 9); kputs(" - "); khex(e[i].base + e[i].length, 9);
        kputs(e[i].type == 1 ? "  usable\n" : "  reserved\n");
    }
}

static void cmd_meminfo(char *a) {
    (void)a;
    uint32_t u, f, b;
    heap_stats(&u, &f, &b);
    kputs("physical: "); kdec(pmm_free_frames() * 4 / 1024); kputs(" MiB free of ");
    kdec(pmm_usable_frames() * 4 / 1024); kputs(" MiB managed\n");
    kputs("heap    : "); kdec(u); kputs(" B used, "); kdec(f); kputs(" B free, "); kdec(b); kputs(" blocks\n");
}

static void cmd_memtest(char *a) {
    (void)a;
    uint32_t u0, f0, b0, u1, f1, b1, ff = pmm_free_frames(), bad = 0;
    heap_stats(&u0, &f0, &b0);
    void *p[32];
    for (int i = 0; i < 32; i++) {
        uint32_t sz = 16 + (uint32_t)i * 40;
        p[i] = kmalloc(sz);
        if (!p[i]) { kputs("memtest FAIL: out of memory\n"); return; }
        memset(p[i], 0xA0 + i, sz);
    }
    for (int i = 0; i < 32; i++) {                       /* nobody overwrote anybody? */
        uint8_t *q = p[i]; uint32_t sz = 16 + (uint32_t)i * 40;
        for (uint32_t j = 0; j < sz; j++) if (q[j] != (uint8_t)(0xA0 + i)) bad++;
    }
    for (int i = 1; i < 32; i += 2) kfree(p[i]);         /* free odd, then even: tests coalescing */
    for (int i = 0; i < 32; i += 2) kfree(p[i]);
    heap_stats(&u1, &f1, &b1);
    uint32_t fa = pmm_alloc_frame(), fb = pmm_alloc_frame();
    kputs("frames: "); khex(fa, 8); kputs(" "); khex(fb, 8); kputc('\n');
    pmm_free_frame(fa); pmm_free_frame(fb);
    if (!bad && u1 == u0 && b1 == b0 && pmm_free_frames() == ff && fa && fb && fa != fb) {
        kset_color(COLOR_GREEN); kputs("memtest OK\n");
    } else { kset_color(COLOR_RED); kputs("memtest FAIL\n"); }
    kset_color(COLOR_GRAY);
}

static void cmd_alloc(char *a) {
    uint32_t n = katou(a);
    int id = -1;
    for (int i = 0; i < 16; i++) if (!slots[i]) { id = i; break; }
    if (id < 0) { kputs("no free slot (max 16)\n"); return; }
    void *p = kmalloc(n);
    if (!p) { kputs("out of memory\n"); return; }
    slots[id] = p;
    kputs("#"); kdec((uint32_t)id); kputs(" -> "); khex((uint32_t)p, 8); kputs("  ("); kdec(n); kputs(" bytes)\n");
}

static void cmd_free(char *a) {
    uint32_t id = katou(a);
    if (id < 16 && slots[id]) { kfree(slots[id]); slots[id] = 0; kputs("freed\n"); }
    else kputs("bad id\n");
}

static void cmd_reboot(char *a) {
    (void)a;
    while (inb(0x64) & 0x02) {}
    outb(0x64, 0xFE);
    for (;;) __asm__ volatile("hlt");
}

static void cmd_halt(char *a) {
    (void)a;
    kputs("halted.\n");
    for (;;) __asm__ volatile("cli; hlt");
}

/* ---------------- command table ---------------- */
static const struct cmd cmds[] = {
    { "help",    "list commands",                 cmd_help   },
    { "clear",   "clear the screen",              cmd_clear  },
    { "echo",    "print the arguments",           cmd_echo   },
    { "uptime",  "time since boot",               cmd_uptime },
    { "mem",     "usable RAM",                    cmd_mem    },
    { "memmap",  "BIOS memory map",               cmd_memmap },
    { "meminfo", "physical + heap usage",         cmd_meminfo},
    { "memtest", "self-test for the allocator",   cmd_memtest},
    { "alloc",   "alloc N: kmalloc N bytes",      cmd_alloc  },
    { "free",    "free ID: kfree allocation #ID", cmd_free   },
    { "reboot",  "restart the machine",           cmd_reboot },
    { "halt",    "stop the CPU",                  cmd_halt   },
};
#define NCMDS ((int)(sizeof(cmds) / sizeof(cmds[0])))

static void cmd_help(char *a) {
    (void)a;
    kputs("commands:\n");
    for (int i = 0; i < NCMDS; i++) {
        kputs("  "); kputs(cmds[i].name);
        int pad = 9; for (const char *s = cmds[i].name; *s; s++) pad--;
        while (pad-- > 0) kputc(' ');
        kputs(cmds[i].help); kputc('\n');
    }
}

/* ---------------- input handling ---------------- */
static char line[80];
static int len;

static void prompt(void) { kset_color(COLOR_CYAN); kputs("kernel> "); kset_color(COLOR_GRAY); }

static void run(char *c) {
    while (*c == ' ') c++;
    if (!*c) return;
    char *args = c;
    while (*args && *args != ' ') args++;                 /* split "name args" */
    if (*args) { *args++ = 0; while (*args == ' ') args++; }
    for (int i = 0; i < NCMDS; i++)
        if (kstreq(c, cmds[i].name)) { cmds[i].fn(args); return; }
    kset_color(COLOR_RED); kputs("unknown command: "); kputs(c); kputc('\n'); kset_color(COLOR_GRAY);
}

static void handle_char(char c) {
    if (c == '\n') { kputc('\n'); line[len] = 0; run(line); len = 0; prompt(); }
    else if (c == '\b') { if (len > 0) { len--; kputc('\b'); } }
    else if (len < (int)sizeof(line) - 1) { line[len++] = c; kputc(c); }
}

void shell_run(void) {
    prompt();
    for (;;) {
        __asm__ volatile("cli");
        if (keyboard_empty()) { __asm__ volatile("sti; hlt"); continue; }
        int c = keyboard_pop();
        __asm__ volatile("sti");
        handle_char((char)c);
    }
}
