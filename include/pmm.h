#ifndef PMM_H
#define PMM_H
#include <stdint.h>
#include "boot_info.h"

#define FRAME_SIZE 4096u

int      pmm_init(const e820_entry_t *map, uint32_t count);   /* 1 = ok */
uint32_t pmm_alloc_frame(void);                               /* physical address, 0 = out of memory */
void     pmm_free_frame(uint32_t addr);
uint32_t pmm_alloc_contig(uint32_t n);                        /* n contiguous frames */
uint32_t pmm_free_frames(void);
uint32_t pmm_usable_frames(void);

#endif
