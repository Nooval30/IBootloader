/* 8259 PIC: remap IRQs away from the CPU exception vectors, mask/unmask lines. */
#include "pic.h"
#include "io.h"

static uint8_t mask_master = 0xFF, mask_slave = 0xFF;

void pic_init(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11);     /* start init sequence */
    outb(0x21, 0x20); outb(0xA1, 0x28);     /* IRQ0-7 -> 0x20.., IRQ8-15 -> 0x28.. */
    outb(0x21, 0x04); outb(0xA1, 0x02);     /* master/slave wiring */
    outb(0x21, 0x01); outb(0xA1, 0x01);     /* 8086 mode */
    outb(0x21, mask_master); outb(0xA1, mask_slave);
}

void pic_unmask(uint8_t irq) {
    if (irq < 8) { mask_master &= (uint8_t)~(1u << irq); outb(0x21, mask_master); }
    else { mask_slave &= (uint8_t)~(1u << (irq - 8)); mask_master &= (uint8_t)~(1u << 2); outb(0x21, mask_master); outb(0xA1, mask_slave); }
}

void pic_eoi(uint8_t irq) {
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}
