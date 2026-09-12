#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>
#include "processor.h"

/*
 * Three-level write-back cache hierarchy:
 *
 *   Per core:   L1-I 128 B + L1-D 128 B + private L2 256 B
 *   Shared:     unified L3 512 B
 *
 * All caches are direct-mapped and use 16-byte cache lines.
 */
#define CACHE_LINE_SIZE       16
#define L1_INSTRUCTION_SIZE   128
#define L1_DATA_SIZE          128
#define L2_SIZE               256
#define L3_SIZE               512

#define L1_INSTRUCTION_LINES  (L1_INSTRUCTION_SIZE / CACHE_LINE_SIZE)
#define L1_DATA_LINES         (L1_DATA_SIZE / CACHE_LINE_SIZE)
#define L2_LINES              (L2_SIZE / CACHE_LINE_SIZE)
#define L3_LINES              (L3_SIZE / CACHE_LINE_SIZE)

void cache_initialize(void);
void cache_finalize(void);

/* isFetch != 0 selects the private L1 instruction cache. */
int cache_read(int proc_id, int isFetch, int physical_address, uint8_t *value);
int cache_write(int proc_id, int physical_address, uint8_t value);

/* Write all dirty cache data belonging to a physical frame back to RAM. */
void cache_writeback_frame(int physical_frame);

/* Write back, then invalidate all cache lines belonging to a physical frame. */
void cache_invalidate_frame(int physical_frame);

/* proc_id >= 0: flush that core's L1 caches into its L2.
 * proc_id == -1: flush the complete hierarchy into RAM. */
void cache_flush(int proc_id);

void cache_print_stats(void);

#endif
