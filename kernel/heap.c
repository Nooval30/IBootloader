/* Kernel heap: first-fit free list with block splitting and coalescing. */
#include "heap.h"

typedef struct blk { uint32_t size, free; struct blk *next, *prev; } blk_t;   /* 16-byte header */

static blk_t *heap_head;
static uint32_t heap_lo, heap_hi;

void heap_init(void *base, uint32_t size) {
    heap_head = (blk_t *)base;
    heap_head->size = size - sizeof(blk_t);
    heap_head->free = 1;
    heap_head->next = heap_head->prev = 0;
    heap_lo = (uint32_t)base; heap_hi = heap_lo + size;
}

void *kmalloc(uint32_t n) {
    if (!heap_head) return 0;
    n = (n + 15) & ~15u; if (!n) n = 16;
    for (blk_t *b = heap_head; b; b = b->next) {
        if (!b->free || b->size < n) continue;
        if (b->size >= n + sizeof(blk_t) + 16) {                 /* split */
            blk_t *nb = (blk_t *)((uint8_t *)(b + 1) + n);
            nb->size = b->size - n - sizeof(blk_t);
            nb->free = 1; nb->next = b->next; nb->prev = b;
            if (b->next) b->next->prev = nb;
            b->next = nb; b->size = n;
        }
        b->free = 0;
        return b + 1;
    }
    return 0;
}

void kfree(void *p) {
    if (!p || (uint32_t)p <= heap_lo || (uint32_t)p >= heap_hi) return;
    blk_t *b = (blk_t *)p - 1;
    if (b->free) return;                                          /* double free: ignore */
    b->free = 1;
    if (b->next && b->next->free) {                               /* merge with next */
        b->size += sizeof(blk_t) + b->next->size;
        b->next = b->next->next;
        if (b->next) b->next->prev = b;
    }
    if (b->prev && b->prev->free) {                               /* merge with previous */
        b->prev->size += sizeof(blk_t) + b->size;
        b->prev->next = b->next;
        if (b->next) b->next->prev = b->prev;
    }
}

void heap_stats(uint32_t *used, uint32_t *freeb, uint32_t *blocks) {
    *used = *freeb = *blocks = 0;
    for (blk_t *b = heap_head; b; b = b->next) {
        (*blocks)++;
        if (b->free) *freeb += b->size; else *used += b->size;
    }
}
