/* System commands: uptime, CPU info, sleep, memory peek. */
#include "commands.h"
#include "console.h"
#include "timer.h"
#include "kstring.h"

static void cmd_uptime(char *a) {
    (void)a;
    uint32_t t = timer_ticks();
    kdec(t / timer_hz()); kputs(" s ("); kdec(t); kputs(" ticks)\n");
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

static const struct shell_cmd cmds[] = {
    { "uptime", "time since boot",                    cmd_uptime },
    { "cpuid",  "CPU vendor and brand",               cmd_cpuid  },
    { "sleep",  "sleep N: wait N seconds",            cmd_sleep  },
    { "peek",   "peek ADDR [N]: hex dump (hex addr)", cmd_peek   },
};
const struct cmd_group group_sys = { cmds, (int)(sizeof(cmds) / sizeof(cmds[0])) };
