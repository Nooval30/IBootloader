/* VGA text console (80x25) mirrored to the COM1 serial port. */
#include "console.h"
#include "io.h"

#define VGA  ((volatile uint16_t *)0xB8000)
#define COM1 0x3F8

static int cx = 0, cy = 14;           /* start below the bootloader's output */
static uint8_t col = COLOR_GRAY;

static void cursor_update(void) {
    uint16_t pos = (uint16_t)(cy * 80 + cx);
    outb(0x3D4, 0x0F); outb(0x3D5, pos & 0xFF);
    outb(0x3D4, 0x0E); outb(0x3D5, pos >> 8);
}
static void ser(char c) { while (!(inb(COM1 + 5) & 0x20)) {} outb(COM1, (uint8_t)c); }

static void scroll(void) {
    for (int i = 0; i < 80 * 24; i++) VGA[i] = VGA[i + 80];
    for (int i = 0; i < 80; i++) VGA[80 * 24 + i] = (uint16_t)(col << 8) | ' ';
    cy = 24;
}

void kset_color(uint8_t c) { col = c; }

void kputc(char c) {
    if (c == '\n') { ser('\r'); ser('\n'); cx = 0; cy++; }
    else if (c == '\b') {
        if (cx > 0) { cx--; VGA[cy * 80 + cx] = (uint16_t)(col << 8) | ' '; ser('\b'); ser(' '); ser('\b'); }
    } else {
        ser(c);
        VGA[cy * 80 + cx] = (uint16_t)(col << 8) | (uint8_t)c;
        if (++cx >= 80) { cx = 0; cy++; }
    }
    if (cy >= 25) scroll();
    cursor_update();
}
void kputs(const char *s) { while (*s) kputc(*s++); }

void kclear(void) {
    for (int i = 0; i < 80 * 25; i++) VGA[i] = (uint16_t)(COLOR_GRAY << 8) | ' ';
    cx = cy = 0; cursor_update();
}
void kdec(uint32_t v) {
    char b[11]; int i = 0;
    if (!v) { kputc('0'); return; }
    while (v) { b[i++] = (char)('0' + v % 10); v /= 10; }
    while (i) kputc(b[--i]);
}
void khex(uint64_t v, int digits) {
    static const char h[] = "0123456789ABCDEF";
    kputs("0x");
    for (int i = (digits - 1) * 4; i >= 0; i -= 4) kputc(h[(v >> i) & 0xF]);
}
