/* PIT timer (IRQ0): a tick counter at a fixed frequency. */
#include "timer.h"
#include "idt.h"
#include "pic.h"
#include "io.h"

static volatile uint32_t ticks;
static uint32_t freq;

static void __attribute__((interrupt)) irq0(struct int_frame *f) {
    (void)f;
    ticks++;
    pic_eoi(0);
}

void timer_init(uint32_t hz) {
    freq = hz;
    uint32_t div = 1193182 / hz;
    outb(0x43, 0x36);
    outb(0x40, div & 0xFF);
    outb(0x40, (div >> 8) & 0xFF);
    idt_set_gate(0x20, (void (*)(void))irq0);
    pic_unmask(0);
}

uint32_t timer_ticks(void) { return ticks; }
uint32_t timer_hz(void) { return freq; }
