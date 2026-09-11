#include <stdio.h>
#include <string.h>

#include "cache.h"
#include "memory.h"

#define CACHE_INVALID_TAG (-1)

typedef struct {
    int valid;
    int tag;
    uint8_t data[CACHE_LINE_SIZE];
} CacheLine;

static CacheLine cache[NP][CACHE_LINES];
static unsigned long cache_hits[NP];
static unsigned long cache_misses[NP];

static int valid_proc(int proc_id)
{
    return proc_id >= 0 && proc_id < NP;
}

static int valid_address(int physical_address)
{
    return physical_address >= 0 && physical_address < MEMSIZE;
}

static int line_number(int physical_address)
{
    return (physical_address / CACHE_LINE_SIZE) % CACHE_LINES;
}

static int tag_for(int physical_address)
{
    return physical_address / CACHE_LINE_SIZE;
}

static int line_base(int physical_address)
{
    return physical_address - (physical_address % CACHE_LINE_SIZE);
}

static void fill_line(int proc_id, int physical_address)
{
    int index = line_number(physical_address);
    int base = line_base(physical_address);

    cache[proc_id][index].valid = 1;
    cache[proc_id][index].tag = tag_for(physical_address);
    memcpy(cache[proc_id][index].data,
           &memory[base],
           CACHE_LINE_SIZE);
}

void cache_initialize(void)
{
    memset(cache, 0, sizeof(cache));
    for (int p = 0; p < NP; p++) {
        for (int i = 0; i < CACHE_LINES; i++)
            cache[p][i].tag = CACHE_INVALID_TAG;
    }
    memset(cache_hits, 0, sizeof(cache_hits));
    memset(cache_misses, 0, sizeof(cache_misses));
}

void cache_finalize(void)
{
    cache_initialize();
}

int cache_read(int proc_id, int physical_address, uint8_t *value)
{
    if (!valid_proc(proc_id) || !valid_address(physical_address) || value == NULL)
        return -1;

    int index = line_number(physical_address);
    int tag = tag_for(physical_address);
    int offset = physical_address % CACHE_LINE_SIZE;

    CacheLine *line = &cache[proc_id][index];

    if (line->valid && line->tag == tag) {
        cache_hits[proc_id]++;
        *value = line->data[offset];
        return 0;
    }

    cache_misses[proc_id]++;
    fill_line(proc_id, physical_address);
    *value = cache[proc_id][index].data[offset];
    return 0;
}

int cache_write(int proc_id, int physical_address, uint8_t value)
{
    if (!valid_proc(proc_id) || !valid_address(physical_address))
        return -1;

    int index = line_number(physical_address);
    int tag = tag_for(physical_address);
    int offset = physical_address % CACHE_LINE_SIZE;

    CacheLine *line = &cache[proc_id][index];

    if (line->valid && line->tag == tag) {
        cache_hits[proc_id]++;
    } else {
        cache_misses[proc_id]++;
        fill_line(proc_id, physical_address);
        line = &cache[proc_id][index];
    }

    /* Write-through: RAM is updated immediately, so swap sees current data. */
    line->data[offset] = value;
    memory[physical_address] = value;
    return 0;
}

void cache_invalidate_frame(int physical_frame)
{
    if (physical_frame <= 0 || physical_frame >= NUM_PHYSICAL_PAGES)
        return;

    int frame_start = physical_frame * PAGESIZE;
    int frame_end = frame_start + PAGESIZE;

    for (int p = 0; p < NP; p++) {
        for (int i = 0; i < CACHE_LINES; i++) {
            if (!cache[p][i].valid)
                continue;

            int cached_start = cache[p][i].tag * CACHE_LINE_SIZE;
            int cached_end = cached_start + CACHE_LINE_SIZE;

            if (cached_start < frame_end && cached_end > frame_start) {
                cache[p][i].valid = 0;
                cache[p][i].tag = CACHE_INVALID_TAG;
            }
        }
    }
}

void cache_flush(int proc_id)
{
    if (proc_id == -1) {
        cache_initialize();
        return;
    }

    if (!valid_proc(proc_id))
        return;

    memset(cache[proc_id], 0, sizeof(cache[proc_id]));
    for (int i = 0; i < CACHE_LINES; i++)
        cache[proc_id][i].tag = CACHE_INVALID_TAG;
}

void cache_print_stats(void)
{
    unsigned long total_hits = 0;
    unsigned long total_misses = 0;

    printf("\n========== L1 CACHE STATISTICS ==========\n");

    for (int p = 0; p < NP; p++) {
        unsigned long accesses = cache_hits[p] + cache_misses[p];
        double hit_rate = accesses
            ? 100.0 * (double)cache_hits[p] / (double)accesses
            : 0.0;

        printf("[CACHE] proc=%d: hits=%lu, misses=%lu, hit rate=%.2f%%\n",
               p, cache_hits[p], cache_misses[p], hit_rate);

        total_hits += cache_hits[p];
        total_misses += cache_misses[p];
    }

    unsigned long total_accesses = total_hits + total_misses;
    double total_hit_rate = total_accesses
        ? 100.0 * (double)total_hits / (double)total_accesses
        : 0.0;

    printf("-----------------------------------------\n");
    printf("[CACHE] total: hits=%lu, misses=%lu, hit rate=%.2f%%\n",
           total_hits, total_misses, total_hit_rate);
    printf("=========================================\n");
}
