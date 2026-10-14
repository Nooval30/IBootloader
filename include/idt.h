#ifndef IDT_H
#define IDT_H
#include <stdint.h>

struct int_frame { uint32_t eip, cs, eflags; };   /* pushed by the CPU on interrupt */

void idt_init(void);                              /* installs CPU exception handlers + loads IDT */
void idt_set_gate(int n, void (*handler)(void));  /* register a handler (IRQs use 0x20..0x2F) */

#endif
