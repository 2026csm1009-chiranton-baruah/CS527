#include <stdio.h>

#include "tlb.h"


/*
 * ============================================================
 * TLB state
 * ============================================================
 */

static TLBEntry tlb[TLB_SIZE];

/*
 * Points to the entry that will be replaced next when the TLB
 * is full.
 */
static int fifo_next = 0;


/*
 * ============================================================
 * initialize_tlb
 * ============================================================
 */

void initialize_tlb(void)
{
    int i;

    for (i = 0; i < TLB_SIZE; i++) {
        tlb[i].valid = 0;
        tlb[i].pid = -1;
        tlb[i].logical_page = -1;
        tlb[i].physical_frame = -1;
    }

    fifo_next = 0;
}


/*
 * ============================================================
 * finalize_tlb
 * ============================================================
 */

void finalize_tlb(void)
{
    /*
     * The TLB contains no dynamically allocated memory.
     * Simply invalidate all entries.
     */
    initialize_tlb();
}


/*
 * ============================================================
 * tlb_lookup
 * ============================================================
 */

int tlb_lookup(int proc_id, int logical_page)
{
    int i;

    for (i = 0; i < TLB_SIZE; i++) {

        if (!tlb[i].valid) {
            continue;
        }

        if (tlb[i].pid == proc_id &&
            tlb[i].logical_page == logical_page) {

            return tlb[i].physical_frame;
        }
    }

    return -1;
}


/*
 * ============================================================
 * tlb_insert
 * ============================================================
 */

void tlb_insert(int proc_id,
                int logical_page,
                int physical_frame)
{
    int i;

    /*
     * Do not insert an invalid physical frame.
     */
    if (proc_id < 0 ||
        proc_id >= NP ||
        logical_page < 0 ||
        physical_frame < 0) {
        return;
    }


    /*
     * If the mapping already exists, update it rather than
     * creating a duplicate entry.
     */
    for (i = 0; i < TLB_SIZE; i++) {

        if (tlb[i].valid &&
            tlb[i].pid == proc_id &&
            tlb[i].logical_page == logical_page) {

            tlb[i].physical_frame = physical_frame;
            return;
        }
    }


    /*
     * Prefer an unused entry before invoking FIFO replacement.
     */
    for (i = 0; i < TLB_SIZE; i++) {

        if (!tlb[i].valid) {

            tlb[i].valid = 1;
            tlb[i].pid = proc_id;
            tlb[i].logical_page = logical_page;
            tlb[i].physical_frame = physical_frame;

            return;
        }
    }


    /*
     * TLB is full.
     *
     * Replace the oldest FIFO entry.
     */
    tlb[fifo_next].valid = 1;
    tlb[fifo_next].pid = proc_id;
    tlb[fifo_next].logical_page = logical_page;
    tlb[fifo_next].physical_frame = physical_frame;

    fifo_next = (fifo_next + 1) % TLB_SIZE;
}


/*
 * ============================================================
 * tlb_invalidate
 * ============================================================
 */

void tlb_invalidate(int proc_id,
                    int logical_page)
{
    int i;

    for (i = 0; i < TLB_SIZE; i++) {

        if (tlb[i].valid &&
            tlb[i].pid == proc_id &&
            tlb[i].logical_page == logical_page) {

            tlb[i].valid = 0;
            tlb[i].pid = -1;
            tlb[i].logical_page = -1;
            tlb[i].physical_frame = -1;
        }
    }
}


/*
 * ============================================================
 * tlb_flush_process
 * ============================================================
 */

void tlb_flush_process(int proc_id)
{
    int i;

    for (i = 0; i < TLB_SIZE; i++) {

        if (tlb[i].valid &&
            tlb[i].pid == proc_id) {

            tlb[i].valid = 0;
            tlb[i].pid = -1;
            tlb[i].logical_page = -1;
            tlb[i].physical_frame = -1;
        }
    }
}


/*
 * ============================================================
 * tlb_print
 * ============================================================
 */

void tlb_print(void)
{
    int i;

    printf("\n========== TLB ==========\n");

    for (i = 0; i < TLB_SIZE; i++) {

        if (tlb[i].valid) {

            printf(
                "Entry %d: PID=%d  LogicalPage=%d  "
                "PhysicalFrame=%d\n",
                i,
                tlb[i].pid,
                tlb[i].logical_page,
                tlb[i].physical_frame
            );

        } else {

            printf("Entry %d: INVALID\n", i);
        }
    }

    printf("=========================\n");
}

