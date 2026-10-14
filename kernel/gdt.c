/* Our own flat GDT (the bootloader's one is just a temporary). */
#include "gdt.h"
#include <stdint.h>

struct gdt_entry { uint16_t limit_lo, base_lo; uint8_t base_mid, access, gran, base_hi; } __attribute__((packed));
struct gdtr { uint16_t limit; uint32_t base; } __attribute__((packed));

static struct gdt_entry gdt[3];

void gdt_init(void) {
    gdt[1] = (struct gdt_entry){ 0xFFFF, 0, 0, 0x9A, 0xCF, 0 };   /* 0x08: code, ring 0 */
    gdt[2] = (struct gdt_entry){ 0xFFFF, 0, 0, 0x92, 0xCF, 0 };   /* 0x10: data, ring 0 */
    struct gdtr r = { sizeof(gdt) - 1, (uint32_t)gdt };
    __asm__ volatile(
        "lgdt %0\n"
        "ljmp $0x08, $1f\n"
        "1: mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n mov %%ax, %%es\n mov %%ax, %%fs\n mov %%ax, %%gs\n mov %%ax, %%ss\n"
        :: "m"(r) : "ax", "memory");
}
