#ifndef TLB_H
#define TLB_H
#include <stdint.h>
#include "processor.h"
#ifndef TLB_ENTRIES
#define TLB_ENTRIES 8
#endif
#define TLB_INVALID_FRAME (-1)
void tlb_initialize(void);
void tlb_finalize(void);
int tlb_lookup(int proc_id, int logical_page);
void tlb_insert(int proc_id, int logical_page, int physical_frame);
void tlb_invalidate(int proc_id, int logical_page);
void tlb_invalidate_frame(int physical_frame);
void tlb_flush(int proc_id);
void tlb_print_status(void);
void tlb_print_stats(void);
#endif
