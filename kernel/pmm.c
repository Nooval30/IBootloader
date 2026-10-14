/* Physical memory manager: 1 bit per 4 KiB frame (1 = used), bitmap placed at 1 MiB. */
#include "pmm.h"

#define BITMAP_ADDR 0x100000u

static uint32_t *bitmap;
static uint32_t nframes, free_frames, usable_frames;

static inline int  frame_used(uint32_t f)  { return bitmap[f >> 5] & (1u << (f & 31)); }
static inline void frame_set(uint32_t f)   { bitmap[f >> 5] |=  (1u << (f & 31)); }
static inline void frame_clear(uint32_t f) { bitmap[f >> 5] &= ~(1u << (f & 31)); }

int pmm_init(const e820_entry_t *map, uint32_t count) {
    uint64_t top = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (map[i].type != 1) continue;
        uint64_t end = map[i].base + map[i].length;
        if (end > 0x100000000ULL) end = 0x100000000ULL;      /* 32-bit kernel: max 4 GiB */
        if (end > top) top = end;
    }
    nframes = (uint32_t)(top >> 12);
    uint32_t words = (nframes + 31) / 32;

    /* the bitmap itself must sit inside a usable region at 1 MiB */
    int ok = 0;
    for (uint32_t i = 0; i < count; i++)
        if (map[i].type == 1 && map[i].base <= BITMAP_ADDR &&
            map[i].base + map[i].length >= (uint64_t)BITMAP_ADDR + words * 4) ok = 1;
    if (!ok) return 0;

    bitmap = (uint32_t *)BITMAP_ADDR;
    for (uint32_t i = 0; i < words; i++) bitmap[i] = 0xFFFFFFFFu;       /* start: all used */

    for (uint32_t i = 0; i < count; i++) {                               /* free usable frames above 1 MiB */
        if (map[i].type != 1) continue;
        uint64_t end = map[i].base + map[i].length;
        if (end > 0x100000000ULL) end = 0x100000000ULL;
        uint32_t s = (uint32_t)((map[i].base + 0xFFF) >> 12), e = (uint32_t)(end >> 12);
        if (s < 0x100) s = 0x100;
        for (uint32_t f = s; f < e; f++)
            if (frame_used(f)) { frame_clear(f); free_frames++; }
    }
    uint32_t bframes = (words * 4 + FRAME_SIZE - 1) / FRAME_SIZE;        /* reserve the bitmap's own frames */
    for (uint32_t f = 0; f < bframes; f++) {
        uint32_t fr = (BITMAP_ADDR >> 12) + f;
        if (!frame_used(fr)) { frame_set(fr); free_frames--; }
    }
    usable_frames = free_frames;
    return 1;
}

uint32_t pmm_alloc_frame(void) {
    uint32_t words = (nframes + 31) / 32;
    for (uint32_t i = 0; i < words; i++) {
        if (bitmap[i] == 0xFFFFFFFFu) continue;
        uint32_t f = i * 32 + (uint32_t)__builtin_ctz(~bitmap[i]);
        if (f >= nframes) break;
        frame_set(f); free_frames--;
        return f << 12;
    }
    return 0;
}

void pmm_free_frame(uint32_t addr) {
    uint32_t f = addr >> 12;
    if (f >= 0x100 && f < nframes && frame_used(f)) { frame_clear(f); free_frames++; }
}

uint32_t pmm_alloc_contig(uint32_t n) {
    uint32_t run = 0, start = 0;
    for (uint32_t f = 0x100; f < nframes; f++) {
        if (frame_used(f)) { run = 0; continue; }
        if (run == 0) start = f;
        if (++run == n) {
            for (uint32_t k = 0; k < n; k++) frame_set(start + k);
            free_frames -= n;
            return start << 12;
        }
    }
    return 0;
}

uint32_t pmm_free_frames(void) { return free_frames; }
uint32_t pmm_usable_frames(void) { return usable_frames; }
