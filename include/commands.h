#ifndef COMMANDS_H
#define COMMANDS_H

/* A shell command: name typed by the user, one-line help, handler (receives the text after the name). */
struct shell_cmd {
    const char *name;
    const char *help;
    void (*fn)(char *args);
};

/* Each cmd_*.c file exports one group of commands. */
struct cmd_group {
    const struct shell_cmd *cmds;
    int count;
};

extern const struct cmd_group group_basic;   /* cmd_basic.c: clear, echo, reboot, halt */
extern const struct cmd_group group_sys;     /* cmd_sys.c  : uptime, cpuid, sleep, peek */
extern const struct cmd_group group_mem;     /* cmd_mem.c  : mem, memmap, meminfo, memtest, alloc, free */

#endif
