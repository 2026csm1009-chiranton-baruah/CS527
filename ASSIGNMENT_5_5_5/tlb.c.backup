#include <stdio.h>
#include <string.h>

#include "tlb.h"

/* A TLB entry maps one process-local logical page to a physical frame. */
typedef struct {
    int valid;
    int logical_page;
    int physical_frame;
    unsigned long stamp;
} TLBEntry;

static TLBEntry tlb[NP][TLB_ENTRIES];
static unsigned long next_stamp;

static int valid_proc(int proc_id)
{
    return proc_id >= 0 && proc_id < NP;
}

static int valid_page(int logical_page)
{
    return logical_page >= 0 && logical_page < 10;
}

static int valid_frame(int physical_frame)
{
    return physical_frame > 0;
}

void tlb_initialize(void)
{
    memset(tlb, 0, sizeof(tlb));
    next_stamp = 1;
}

void tlb_finalize(void)
{
    memset(tlb, 0, sizeof(tlb));
    next_stamp = 1;
}

int tlb_lookup(int proc_id, int logical_page)
{
    if (!valid_proc(proc_id) || !valid_page(logical_page))
        return TLB_INVALID_FRAME;

    for (int i = 0; i < TLB_ENTRIES; i++) {
        if (tlb[proc_id][i].valid &&
            tlb[proc_id][i].logical_page == logical_page) {
            tlb[proc_id][i].stamp = next_stamp++;
            return tlb[proc_id][i].physical_frame;
        }
    }

    return TLB_INVALID_FRAME;
}

void tlb_insert(int proc_id, int logical_page, int physical_frame)
{
    if (!valid_proc(proc_id) || !valid_page(logical_page) ||
        !valid_frame(physical_frame))
        return;

    int victim = 0;
    unsigned long oldest = ~0UL;

    for (int i = 0; i < TLB_ENTRIES; i++) {
        if (tlb[proc_id][i].valid &&
            tlb[proc_id][i].logical_page == logical_page) {
            victim = i;
            goto install;
        }

        if (!tlb[proc_id][i].valid) {
            victim = i;
            oldest = 0;
            break;
        }

        if (tlb[proc_id][i].stamp < oldest) {
            oldest = tlb[proc_id][i].stamp;
            victim = i;
        }
    }

install:
    tlb[proc_id][victim].valid = 1;
    tlb[proc_id][victim].logical_page = logical_page;
    tlb[proc_id][victim].physical_frame = physical_frame;
    tlb[proc_id][victim].stamp = next_stamp++;
}

void tlb_invalidate(int proc_id, int logical_page)
{
    if (!valid_proc(proc_id) || !valid_page(logical_page))
        return;

    for (int i = 0; i < TLB_ENTRIES; i++) {
        if (tlb[proc_id][i].valid &&
            tlb[proc_id][i].logical_page == logical_page) {
            tlb[proc_id][i].valid = 0;
            tlb[proc_id][i].physical_frame = TLB_INVALID_FRAME;
        }
    }
}

void tlb_invalidate_frame(int physical_frame)
{
    if (!valid_frame(physical_frame))
        return;

    for (int p = 0; p < NP; p++) {
        for (int i = 0; i < TLB_ENTRIES; i++) {
            if (tlb[p][i].valid &&
                tlb[p][i].physical_frame == physical_frame) {
                tlb[p][i].valid = 0;
                tlb[p][i].physical_frame = TLB_INVALID_FRAME;
            }
        }
    }
}

void tlb_flush(int proc_id)
{
    if (proc_id == -1) {
        memset(tlb, 0, sizeof(tlb));
        return;
    }

    if (!valid_proc(proc_id))
        return;

    memset(tlb[proc_id], 0, sizeof(tlb[proc_id]));
}

void tlb_print_status(void)
{
    printf("[TLB] %d entries per processor\n", TLB_ENTRIES);

    for (int p = 0; p < NP; p++) {
        printf("[TLB] proc=%d:", p);
        for (int i = 0; i < TLB_ENTRIES; i++) {
            if (tlb[p][i].valid)
                printf(" [page=%d->frame=%d]",
                       tlb[p][i].logical_page,
                       tlb[p][i].physical_frame);
        }
        printf("\n");
    }
}
