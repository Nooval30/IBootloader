#ifndef PIC_H
#define PIC_H
#include <stdint.h>

void pic_init(void);              /* remap IRQs to 0x20..0x2F, mask all */
void pic_unmask(uint8_t irq);     /* enable one IRQ line (0..15) */
void pic_eoi(uint8_t irq);        /* end-of-interrupt, call at the end of every IRQ handler */

#endif
