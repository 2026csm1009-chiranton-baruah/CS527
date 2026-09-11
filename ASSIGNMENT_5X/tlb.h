#ifndef TLB_H
#define TLB_H

#include <stdint.h>

#include "processor.h"

/*
 * Small fully-associative TLB used by the simulator.
 * Each simulated processor owns its own TLB.
 */
#ifndef TLB_ENTRIES
#define TLB_ENTRIES 8
#endif

#define TLB_INVALID_FRAME (-1)

void tlb_initialize(void);
void tlb_finalize(void);

/* Return the cached physical frame, or TLB_INVALID_FRAME on a miss. */
int tlb_lookup(int proc_id, int logical_page);

/* Insert/replace a translation. */
void tlb_insert(int proc_id, int logical_page, int physical_frame);

/* Remove one translation. */
void tlb_invalidate(int proc_id, int logical_page);

/* Remove every cached translation belonging to a physical frame. */
void tlb_invalidate_frame(int physical_frame);

/* Flush one processor's TLB, or all TLBs with proc_id = -1. */
void tlb_flush(int proc_id);

void tlb_print_status(void);

/* Print per-processor and total TLB hit/miss statistics. */
void tlb_print_stats(void);

#endif
