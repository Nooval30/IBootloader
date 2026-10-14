/* Minimal freestanding string/memory helpers. */
#include "kstring.h"

/* gcc may emit calls to memset/memcpy itself, so they must exist */
__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *memset(void *d, int v, uint32_t n) { uint8_t *p = d; while (n--) *p++ = (uint8_t)v; return d; }

__attribute__((optimize("no-tree-loop-distribute-patterns")))
void *memcpy(void *d, const void *s, uint32_t n) { uint8_t *a = d; const uint8_t *b = s; while (n--) *a++ = *b++; return d; }

int kstreq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }

int kstarts(const char *s, const char *prefix) { while (*prefix) if (*s++ != *prefix++) return 0; return 1; }

uint32_t katou(const char *s) {
    uint32_t v = 0;
    while (*s >= '0' && *s <= '9') v = v * 10 + (uint32_t)(*s++ - '0');
    return v;
}
