/* IDT + CPU exception handlers (vectors 0..31). */
#include "idt.h"
#include "console.h"

struct idt_entry { uint16_t off_lo, sel; uint8_t zero, flags; uint16_t off_hi; } __attribute__((packed));
struct idtr { uint16_t limit; uint32_t base; } __attribute__((packed));

static struct idt_entry idt[256];

void idt_set_gate(int n, void (*handler)(void)) {
    uint32_t a = (uint32_t)handler;
    idt[n] = (struct idt_entry){ a & 0xFFFF, 0x08, 0, 0x8E, a >> 16 };
}

static void exc_panic(int n) {
    kset_color(COLOR_PANIC);
    kputs("\nCPU EXCEPTION #"); kdec((uint32_t)n); kputs(" - halted\n");
    for (;;) __asm__ volatile("cli; hlt");
}

/* exceptions 8, 10-14, 17, 21, 29, 30 push an error code */
#define EXC(n)     static void __attribute__((interrupt)) exc##n(struct int_frame *f) { (void)f; exc_panic(n); }
#define EXC_ERR(n) static void __attribute__((interrupt)) exc##n(struct int_frame *f, uint32_t e) { (void)f; (void)e; exc_panic(n); }
EXC(0) EXC(1) EXC(2) EXC(3) EXC(4) EXC(5) EXC(6) EXC(7) EXC_ERR(8) EXC(9)
EXC_ERR(10) EXC_ERR(11) EXC_ERR(12) EXC_ERR(13) EXC_ERR(14) EXC(15) EXC(16) EXC_ERR(17) EXC(18) EXC(19)
EXC(20) EXC_ERR(21) EXC(22) EXC(23) EXC(24) EXC(25) EXC(26) EXC(27) EXC(28) EXC_ERR(29) EXC_ERR(30) EXC(31)

#define E(n) (void (*)(void))exc##n
static void (*const exc_table[32])(void) = {
    E(0),E(1),E(2),E(3),E(4),E(5),E(6),E(7),E(8),E(9),E(10),E(11),E(12),E(13),E(14),E(15),
    E(16),E(17),E(18),E(19),E(20),E(21),E(22),E(23),E(24),E(25),E(26),E(27),E(28),E(29),E(30),E(31)
};

void idt_init(void) {
    for (int i = 0; i < 32; i++) idt_set_gate(i, exc_table[i]);
    struct idtr r = { sizeof(idt) - 1, (uint32_t)idt };
    __asm__ volatile("lidt %0" :: "m"(r));
}
