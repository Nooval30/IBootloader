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

static void put_byte(uint8_t v) {
    static const char h[] = "0123456789ABCDEF";
    kputc(h[v >> 4]); kputc(h[v & 15]);
}

static void cmd_peek(char *a) {                         /* peek ADDR [N] : hex dump of memory */
    if (!*a) { kputs("usage: peek ADDR [N]   (ADDR in hex, N <= 256)\n"); return; }
    uint32_t addr = katoh(a), n = 16;
    while (*a && *a != ' ') a++;
    while (*a == ' ') a++;
    if (*a) n = katou(a);
    if (n == 0) n = 16;
    if (n > 256) n = 256;
    for (uint32_t off = 0; off < n; off += 16) {
        khex(addr + off, 8); kputs(": ");
        for (uint32_t i = 0; i < 16; i++) {
            if (off + i < n) { put_byte(*(volatile uint8_t *)(addr + off + i)); kputc(' '); }
            else kputs("   ");
        }
        for (uint32_t i = 0; i < 16 && off + i < n; i++) {
            uint8_t c = *(volatile uint8_t *)(addr + off + i);
            kputc(c >= 32 && c < 127 ? (char)c : '.');
        }
        kputc('\n');
    }
}

static void cpuid(uint32_t leaf, uint32_t r[4]) {
    __asm__ volatile("cpuid" : "=a"(r[0]), "=b"(r[1]), "=c"(r[2]), "=d"(r[3]) : "a"(leaf), "c"(0));
}

static void cmd_cpuid(char *a) {
    (void)a;
    uint32_t r[4];
    char v[13];
    cpuid(0, r);
    memcpy(v, &r[1], 4); memcpy(v + 4, &r[3], 4); memcpy(v + 8, &r[2], 4); v[12] = 0;
    kputs("vendor: "); kputs(v); kputc('\n');
    cpuid(0x80000000, r);
    if (r[0] >= 0x80000004) {
        char name[49];
        for (uint32_t i = 0; i < 3; i++) { cpuid(0x80000002 + i, r); memcpy(name + i * 16, r, 16); }
        name[48] = 0;
        const char *s = name; while (*s == ' ') s++;
        kputs("brand : "); kputs(s); kputc('\n');
    }
}

static void cmd_sleep(char *a) {
    uint32_t s = katou(a);
    if (s > 3600) s = 3600;
    timer_sleep(s * timer_hz());
    kputs("done\n");
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
    { "peek",    "peek ADDR [N]: hex dump (hex addr)", cmd_peek },
    { "cpuid",   "CPU vendor and brand",          cmd_cpuid  },
    { "sleep",   "sleep N: wait N seconds",       cmd_sleep  },
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

/* ---- history (last 8 lines, up/down arrows) ---- */
#define HIST_N 8
static char hist[HIST_N][80];
static int hist_head, hist_len;          /* next slot to write, number of stored lines */
static int hist_sel = -1;                /* -1 = typing a new line, k = k steps back */
static char saved[80];                   /* the line being typed while browsing history */

static const char *hist_get(int back) { return hist[(hist_head - 1 - back + 2 * HIST_N) % HIST_N]; }

static void hist_add(const char *l) {
    if (!*l) return;
    if (hist_len && kstreq(hist_get(0), l)) return;      /* skip repeats */
    memcpy(hist[hist_head], l, kstrlen(l) + 1);
    hist_head = (hist_head + 1) % HIST_N;
    if (hist_len < HIST_N) hist_len++;
}

static void set_line(const char *s) {                    /* replace what is typed on screen */
    while (len > 0) { len--; kputc('\b'); }
    while (*s && len < (int)sizeof(line) - 1) { line[len++] = *s; kputc(*s++); }
}

static void handle_key(int c) {
    if (c == KEY_UP) {
        if (hist_sel + 1 < hist_len) {
            if (hist_sel == -1) { line[len] = 0; memcpy(saved, line, (uint32_t)len + 1); }
            hist_sel++;
            set_line(hist_get(hist_sel));
        }
    } else if (c == KEY_DOWN) {
        if (hist_sel >= 0) {
            hist_sel--;
            set_line(hist_sel < 0 ? saved : hist_get(hist_sel));
        }
    } else if (c == '\n') {
        kputc('\n'); line[len] = 0;
        hist_add(line); hist_sel = -1;
        run(line); len = 0; prompt();
    } else if (c == '\b') { if (len > 0) { len--; kputc('\b'); } }
    else if (c < 0x100 && len < (int)sizeof(line) - 1) { line[len++] = (char)c; kputc((char)c); }
}

void shell_run(void) {
    prompt();
    for (;;) {
        __asm__ volatile("cli");
        if (keyboard_empty()) { __asm__ volatile("sti; hlt"); continue; }
        int c = keyboard_pop();
        __asm__ volatile("sti");
        handle_key(c);
    }
}
