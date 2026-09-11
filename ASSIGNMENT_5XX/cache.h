#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>
#include "processor.h"

/*
 * Cache hierarchy:
 *
 *   Per-core L1 Instruction Cache : 128 B
 *   Per-core L1 Data Cache        : 128 B
 *   Unified shared L2 Cache       : 512 B
 *
 * All caches use 16-byte cache lines.
 *
 * L1-I : write-back / write-allocate
 * L1-D : write-back / write-allocate
 * L2   : write-back / write-allocate
 *
 * L2 is shared by all processors.
 * Physical memory is the backing store for L2.
 */

#define CACHE_LINE_SIZE       16

#define L1_INSTRUCTION_SIZE   128
#define L1_DATA_SIZE          128
#define L2_SIZE               512

#define L1_INSTRUCTION_LINES \
    (L1_INSTRUCTION_SIZE / CACHE_LINE_SIZE)

#define L1_DATA_LINES \
    (L1_DATA_SIZE / CACHE_LINE_SIZE)

#define L2_LINES \
    (L2_SIZE / CACHE_LINE_SIZE)


void cache_initialize(void);

void cache_finalize(void);


/*
 * Read one byte through the cache hierarchy.
 *
 * isFetch != 0:
 *     use the processor's L1 instruction cache.
 *
 * isFetch == 0:
 *     use the processor's L1 data cache.
 */
int cache_read(int proc_id,
               int isFetch,
               int physical_address,
               uint8_t *value);


/*
 * Write one byte through the processor's L1 data cache.
 *
 * Writes are write-back, so physical memory is not immediately
 * modified.
 */
int cache_write(int proc_id,
                int physical_address,
                uint8_t value);


/*
 * Write all dirty cache data belonging to a physical frame
 * back through the cache hierarchy and ultimately into RAM.
 *
 * This must be done before a physical frame is:
 *   - swapped out,
 *   - freed,
 *   - reused for another page.
 */
void cache_writeback_frame(int physical_frame);


/*
 * Write back and invalidate every cache line belonging to
 * the specified physical frame.
 */
void cache_invalidate_frame(int physical_frame);


/*
 * Flush caches.
 *
 * proc_id >= 0:
 *     flush that processor's private L1 caches.
 *
 * proc_id == -1:
 *     flush the complete cache hierarchy.
 */
void cache_flush(int proc_id);


/*
 * Print cache hit/miss statistics.
 */
void cache_print_stats(void);

#endif
