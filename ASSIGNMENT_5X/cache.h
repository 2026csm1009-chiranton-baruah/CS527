#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>
#include "processor.h"

/* Small per-processor L1 cache. */
#define CACHE_LINE_SIZE 16
#define CACHE_LINES      16

void cache_initialize(void);
void cache_finalize(void);

int cache_read(int proc_id, int physical_address, uint8_t *value);
int cache_write(int proc_id, int physical_address, uint8_t value);

/* Invalidate every cache line containing a physical frame. */
void cache_invalidate_frame(int physical_frame);

void cache_flush(int proc_id);
void cache_print_stats(void);

#endif
