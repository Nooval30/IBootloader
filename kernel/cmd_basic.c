/* Basic shell commands. */
#include "commands.h"
#include "console.h"
#include "io.h"

static void cmd_clear(char *a) { (void)a; kclear(); }

static void cmd_echo(char *a) { kputs(a); kputc('\n'); }

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

static const struct shell_cmd cmds[] = {
    { "clear",  "clear the screen",    cmd_clear  },
    { "echo",   "print the arguments", cmd_echo   },
    { "reboot", "restart the machine", cmd_reboot },
    { "halt",   "stop the CPU",        cmd_halt   },
};
const struct cmd_group group_basic = { cmds, (int)(sizeof(cmds) / sizeof(cmds[0])) };
