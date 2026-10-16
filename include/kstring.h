#ifndef KSTRING_H
#define KSTRING_H
#include <stdint.h>

void *memset(void *d, int v, uint32_t n);
void *memcpy(void *d, const void *s, uint32_t n);
int kstreq(const char *a, const char *b);
int kstarts(const char *s, const char *prefix);
uint32_t katou(const char *s);                 /* decimal */
uint32_t katoh(const char *s);                 /* hex, optional 0x prefix */
uint32_t kstrlen(const char *s);

#endif
