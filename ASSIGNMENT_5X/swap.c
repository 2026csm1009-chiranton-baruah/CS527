#include <stdio.h>
#include <string.h>

#include "swap.h"
#include "disk.h"

static int swap_table[NP][SWAP_NUM_LOGICAL_PAGES];
static int initialized;

static int valid_proc(int proc_id)
{
    return proc_id >= 0 && proc_id < NP;
}

static int valid_page(int logical_page)
{
    return logical_page >= 0 && logical_page < SWAP_NUM_LOGICAL_PAGES;
}

void swap_initialize(void)
{
    for (int p = 0; p < NP; p++)
        for (int page = 0; page < SWAP_NUM_LOGICAL_PAGES; page++)
            swap_table[p][page] = SWAP_INVALID_BLOCK;

    initialized = 1;
}

void swap_finalize(void)
{
    if (initialized)
        swap_release_process(-1);

    initialized = 0;
}

int swap_allocate(int proc_id, int logical_page)
{
    if (!initialized || !valid_proc(proc_id) || !valid_page(logical_page))
        return SWAP_INVALID_BLOCK;

    if (swap_table[proc_id][logical_page] >= 0)
        return swap_table[proc_id][logical_page];

    int block = disk_allocate_block();
    if (block < 0)
        return SWAP_INVALID_BLOCK;

    swap_table[proc_id][logical_page] = block;
    return block;
}

int swap_block(int proc_id, int logical_page)
{
    if (!initialized || !valid_proc(proc_id) || !valid_page(logical_page))
        return SWAP_INVALID_BLOCK;

    return swap_table[proc_id][logical_page];
}

int swap_has_page(int proc_id, int logical_page)
{
    return swap_block(proc_id, logical_page) >= 0;
}

int swap_read_page(int proc_id, int logical_page, uint8_t *buffer)
{
    if (!initialized || buffer == NULL)
        return -1;

    int block = swap_block(proc_id, logical_page);
    if (block < 0)
        return -1;

    return disk_read_block(block, buffer) == DISK_OK ? 0 : -1;
}

int swap_write_page(int proc_id, int logical_page, const uint8_t *buffer)
{
    if (!initialized || buffer == NULL)
        return -1;

    int block = swap_block(proc_id, logical_page);
    if (block < 0)
        block = swap_allocate(proc_id, logical_page);

    if (block < 0)
        return -1;

    return disk_write_block(block, buffer) == DISK_OK ? 0 : -1;
}

int swap_free(int proc_id, int logical_page)
{
    if (!initialized || !valid_proc(proc_id) || !valid_page(logical_page))
        return -1;

    int block = swap_table[proc_id][logical_page];
    if (block < 0)
        return 0;

    if (disk_free_block(block) != DISK_OK)
        return -1;

    swap_table[proc_id][logical_page] = SWAP_INVALID_BLOCK;
    return 0;
}

void swap_release_process(int proc_id)
{
    if (!initialized)
        return;

    if (proc_id == -1) {
        for (int p = 0; p < NP; p++)
            swap_release_process(p);
        return;
    }

    if (!valid_proc(proc_id))
        return;

    for (int page = 0; page < SWAP_NUM_LOGICAL_PAGES; page++)
        (void)swap_free(proc_id, page);
}

void swap_print_status(void)
{
    if (!initialized) {
        printf("[SWAP] not initialized\n");
        return;
    }

    int used = 0;
    for (int p = 0; p < NP; p++) {
        for (int page = 0; page < SWAP_NUM_LOGICAL_PAGES; page++) {
            if (swap_table[p][page] >= 0)
                used++;
        }
    }

    printf("[SWAP] %d logical-page slots in use\n", used);
}
