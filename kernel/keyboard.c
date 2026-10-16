/* PS/2 keyboard (IRQ1): scancode set 1 -> ASCII, with shift and caps lock. */
#include "keyboard.h"
#include "idt.h"
#include "pic.h"
#include "io.h"

static const char km_lo[] = "\0\0" "1234567890-=" "\b\t" "qwertyuiop[]" "\n\0" "asdfghjkl;'" "`\0" "\\" "zxcvbnm,./" "\0*\0 ";
static const char km_up[] = "\0\0" "!@#$%^&*()_+" "\b\t" "QWERTYUIOP{}" "\n\0" "ASDFGHJKL:\"" "~\0" "|" "ZXCVBNM<>?" "\0*\0 ";

static volatile uint16_t buf[256];             /* ring buffer, uint8_t indexes wrap by themselves */
static volatile uint8_t head, tail;
static int shift, caps, ext;

static void push(int c) {
    uint8_t n = (uint8_t)(head + 1);
    if (n != tail) { buf[head] = (uint16_t)c; head = n; }
}

static void __attribute__((interrupt)) irq1(struct int_frame *f) {
    (void)f;
    uint8_t sc = inb(0x60);
    if (sc == 0xE0) ext = 1;
    else if (ext) {                                         /* 0xE0-prefixed key */
        ext = 0;
        if (sc == 0x48) push(KEY_UP);
        else if (sc == 0x50) push(KEY_DOWN);                 /* others (and releases) ignored */
    }
    else if (sc == 0x2A || sc == 0x36) shift = 1;
    else if (sc == 0xAA || sc == 0xB6) shift = 0;
    else if (sc == 0x3A) caps ^= 1;
    else if (!(sc & 0x80) && sc < sizeof(km_lo) - 1) {
        char c = shift ? km_up[sc] : km_lo[sc];
        if (caps && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) c ^= 0x20;
        if (c && c != '\t') push(c);
    }
    pic_eoi(1);
}

void keyboard_init(void) {
    idt_set_gate(0x21, (void (*)(void))irq1);
    pic_unmask(1);
}

int keyboard_empty(void) { return head == tail; }

int keyboard_pop(void) {
    if (head == tail) return -1;
    int c = buf[tail];
    tail = (uint8_t)(tail + 1);
    return c;
}
