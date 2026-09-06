#ifndef TLB_H
#define TLB_H

#include "processor.h"

/*
 * ============================================================
 * TLB configuration
 * ============================================================
 */

#define TLB_SIZE 8


/*
 * ============================================================
 * TLB entry
 * ============================================================
 */

typedef struct {
    int valid;
    int pid;
    int logical_page;
    int physical_frame;
} TLBEntry;


/*
 * ============================================================
 * TLB lifecycle
 * ============================================================
 */

void initialize_tlb(void);

void finalize_tlb(void);


/*
 * ============================================================
 * TLB lookup
 * ============================================================
 *
 * Returns:
 *
 *     physical frame number on HIT
 *     -1 on MISS
 * ============================================================
 */

int tlb_lookup(int proc_id,
               int logical_page);


/*
 * ============================================================
 * TLB insertion
 * ============================================================
 */

void tlb_insert(int proc_id,
                int logical_page,
                int physical_frame);


/*
 * ============================================================
 * TLB invalidation
 * ============================================================
 */

void tlb_invalidate(int proc_id,
                    int logical_page);

void tlb_flush_process(int proc_id);


/*
 * ============================================================
 * Debugging
 * ============================================================
 */

void tlb_print(void);

#endif

