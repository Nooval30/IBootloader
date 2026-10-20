/* Shell core: line editing, history, command lookup. The commands themselves live in cmd_*.c.
 * To add a command: put cmd_xxx() + a line in the table of a cmd_*.c file (or make a new group
 * in commands.h and add it to groups[] below). */
#include "shell.h"
#include "commands.h"
#include "console.h"
#include "keyboard.h"
#include "kstring.h"

static const struct cmd_group *const groups[] = { &group_basic, &group_sys, &group_mem };
#define NGROUPS ((int)(sizeof(groups) / sizeof(groups[0])))

/* ---------------- help (needs to see every group, so it lives here) ---------------- */
static void print_cmd(const char *name, const char *help) {
    kputs("  "); kputs(name);
    int pad = 9; for (const char *s = name; *s; s++) pad--;
    while (pad-- > 0) kputc(' ');
    kputs(help); kputc('\n');
}

static void cmd_help(void) {
    kputs("commands:\n");
    print_cmd("help", "list commands");
    for (int g = 0; g < NGROUPS; g++)
        for (int i = 0; i < groups[g]->count; i++)
            print_cmd(groups[g]->cmds[i].name, groups[g]->cmds[i].help);
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
    if (kstreq(c, "help")) { cmd_help(); return; }
    for (int g = 0; g < NGROUPS; g++)
        for (int i = 0; i < groups[g]->count; i++)
            if (kstreq(c, groups[g]->cmds[i].name)) { groups[g]->cmds[i].fn(args); return; }
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
