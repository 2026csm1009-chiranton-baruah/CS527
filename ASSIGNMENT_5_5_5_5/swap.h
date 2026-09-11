#ifndef SWAP_H
#define SWAP_H

#include <stdint.h>

#include "processor.h"

/*
 * Swap space is implemented as disk-backed slots, one slot per
 * logical process page.  The disk subsystem must be initialized
 * before swap_initialize() is called.
 */
#ifndef SWAP_NUM_LOGICAL_PAGES
#define SWAP_NUM_LOGICAL_PAGES 10
#endif

#define SWAP_INVALID_BLOCK (-1)

void swap_initialize(void);
void swap_finalize(void);

/* Allocate a backing slot for a logical page. */
int swap_allocate(int proc_id, int logical_page);

/* Return the disk block backing the page, or -1 if none exists. */
int swap_block(int proc_id, int logical_page);

int swap_has_page(int proc_id, int logical_page);

/* Copy exactly one page between RAM and swap. */
int swap_read_page(int proc_id, int logical_page, uint8_t *buffer);
int swap_write_page(int proc_id, int logical_page, const uint8_t *buffer);

/* Release a page's swap slot. */
int swap_free(int proc_id, int logical_page);

/* Release every slot belonging to one process, or all with proc_id = -1. */
void swap_release_process(int proc_id);

void swap_print_status(void);

#endif
