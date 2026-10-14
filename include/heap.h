#ifndef HEAP_H
#define HEAP_H
#include <stdint.h>

#define HEAP_FRAMES 1024u         /* kernel heap = 1024 * 4 KiB = 4 MiB */

void  heap_init(void *base, uint32_t size);
void *kmalloc(uint32_t n);
void  kfree(void *p);
void  heap_stats(uint32_t *used, uint32_t *freeb, uint32_t *blocks);

#endif
