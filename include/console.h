#ifndef CONSOLE_H
#define CONSOLE_H
#include <stdint.h>

#define COLOR_GRAY   0x07
#define COLOR_GREEN  0x0A
#define COLOR_CYAN   0x0B
#define COLOR_RED    0x0C
#define COLOR_PANIC  0x4F

void kputc(char c);
void kputs(const char *s);
void kdec(uint32_t v);
void khex(uint64_t v, int digits);
void kclear(void);
void kset_color(uint8_t c);

#endif
