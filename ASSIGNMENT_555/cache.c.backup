#include <stdio.h>
#include <string.h>

#include "cache.h"
#include "memory.h"

#define CACHE_INVALID_TAG (-1)

typedef struct {
    int valid;
    int dirty;
    int tag;
    uint8_t data[CACHE_LINE_SIZE];
} CacheLine;

static CacheLine l1_instruction[NP][L1_INSTRUCTION_LINES];
static CacheLine l1_data[NP][L1_DATA_LINES];
static CacheLine l2[NP][L2_LINES];
static CacheLine l3[L3_LINES];

static unsigned long l1i_hits[NP];
static unsigned long l1i_misses[NP];
static unsigned long l1d_hits[NP];
static unsigned long l1d_misses[NP];
static unsigned long l2_hits[NP];
static unsigned long l2_misses[NP];
static unsigned long l3_hits;
static unsigned long l3_misses;

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
    return physical_address / CACHE_LINE_SIZE;
}

static int line_offset(int physical_address)
{
    return physical_address % CACHE_LINE_SIZE;
}

static int line_base_from_number(int line)
{
    return line * CACHE_LINE_SIZE;
}

static int cache_index(int line, int lines)
{
    return line % lines;
}

static int cache_tag(int line, int lines)
{
    return line / lines;
}

static int cached_line_number(const CacheLine *line,
                              int index,
                              int lines)
{
    if (line == NULL || !line->valid)
        return -1;

    return line->tag * lines + index;
}

static void clear_line(CacheLine *line)
{
    if (line == NULL)
        return;

    memset(line, 0, sizeof(*line));
    line->tag = CACHE_INVALID_TAG;
}

static void initialize_lines(CacheLine *lines, int count)
{
    for (int i = 0; i < count; i++)
        clear_line(&lines[i]);
}

/*
 * Write a dirty L3 line to physical RAM.
 */
static void writeback_l3_line(int index)
{
    if (index < 0 || index >= L3_LINES)
        return;

    CacheLine *line = &l3[index];
    if (!line->valid || !line->dirty)
        return;

    int line_no = cached_line_number(line, index, L3_LINES);
    int base = line_base_from_number(line_no);

    if (base >= 0 && base + CACHE_LINE_SIZE <= MEMSIZE)
        memcpy(&memory[base], line->data, CACHE_LINE_SIZE);

    line->dirty = 0;
}

/*
 * Install a complete cache line into the shared L3.
 * If the destination contains dirty data, preserve it in RAM first.
 */
static void l3_install_line(int line_no, const uint8_t *data, int dirty)
{
    if (line_no < 0 || line_no >= MEMSIZE / CACHE_LINE_SIZE || data == NULL)
        return;

    int index = cache_index(line_no, L3_LINES);
    CacheLine *line = &l3[index];

    if (line->valid && line->tag != cache_tag(line_no, L3_LINES))
        writeback_l3_line(index);

    line->valid = 1;
    line->dirty = dirty ? 1 : 0;
    line->tag = cache_tag(line_no, L3_LINES);
    memcpy(line->data, data, CACHE_LINE_SIZE);
}

/*
 * Find a line in the shared L3 without changing statistics.
 */
static int l3_lookup_line(int line_no, uint8_t *data)
{
    if (line_no < 0 || line_no >= MEMSIZE / CACHE_LINE_SIZE || data == NULL)
        return -1;

    int index = cache_index(line_no, L3_LINES);
    CacheLine *line = &l3[index];

    if (!line->valid || line->tag != cache_tag(line_no, L3_LINES))
        return -1;

    memcpy(data, line->data, CACHE_LINE_SIZE);
    return 0;
}

/*
 * Return a line from L3, or from physical RAM on an L3 miss.
 * The fetched line is installed in L3.
 */
static int lower_level_read_line(int line_no, uint8_t *data)
{
    if (line_no < 0 || line_no >= MEMSIZE / CACHE_LINE_SIZE || data == NULL)
        return -1;

    if (l3_lookup_line(line_no, data) == 0) {
        l3_hits++;
        return 0;
    }

    l3_misses++;

    int base = line_base_from_number(line_no);
    if (base < 0 || base + CACHE_LINE_SIZE > MEMSIZE)
        return -1;

    memcpy(data, &memory[base], CACHE_LINE_SIZE);
    l3_install_line(line_no, data, 0);
    return 0;
}

/*
 * Write one private-L2 line to the shared L3. This is the L2 -> L3
 * write-back path and therefore preserves dirty state.
 */
static void writeback_l2_line(int proc_id, int index)
{
    if (!valid_proc(proc_id) || index < 0 || index >= L2_LINES)
        return;

    CacheLine *line = &l2[proc_id][index];
    if (!line->valid || !line->dirty)
        return;

    int line_no = cached_line_number(line, index, L2_LINES);
    if (line_no < 0)
        return;

    l3_install_line(line_no, line->data, 1);
    line->dirty = 0;
}

/*
 * Install/fetch a line in a core's private L2.
 * On an L2 miss, the shared L3 is checked before RAM.
 */
static int l2_get_line(int proc_id, int line_no, uint8_t *data)
{
    if (!valid_proc(proc_id) || data == NULL ||
        line_no < 0 || line_no >= MEMSIZE / CACHE_LINE_SIZE)
        return -1;

    int index = cache_index(line_no, L2_LINES);
    int tag = cache_tag(line_no, L2_LINES);
    CacheLine *line = &l2[proc_id][index];

    if (line->valid && line->tag == tag) {
        l2_hits[proc_id]++;
        memcpy(data, line->data, CACHE_LINE_SIZE);
        return 0;
    }

    l2_misses[proc_id]++;

    /* Evict the private L2 victim before replacing it. */
    if (line->valid)
        writeback_l2_line(proc_id, index);

    if (lower_level_read_line(line_no, data) != 0)
        return -1;

    line->valid = 1;
    line->dirty = 0;
    line->tag = tag;
    memcpy(line->data, data, CACHE_LINE_SIZE);
    return 0;
}

/*
 * Write a dirty L1 line into the core's private L2.
 */
static void writeback_l1_line(int proc_id,
                              CacheLine *line,
                              int index,
                              int lines)
{
    if (!valid_proc(proc_id) || line == NULL || !line->valid || !line->dirty)
        return;

    int line_no = cached_line_number(line, index, lines);
    if (line_no < 0)
        return;

    int l2_index = cache_index(line_no, L2_LINES);
    int l2_tag = cache_tag(line_no, L2_LINES);
    CacheLine *destination = &l2[proc_id][l2_index];

    /* A dirty L2 victim must continue down to L3. */
    if (destination->valid && destination->tag != l2_tag)
        writeback_l2_line(proc_id, l2_index);

    destination->valid = 1;
    destination->dirty = 1;
    destination->tag = l2_tag;
    memcpy(destination->data, line->data, CACHE_LINE_SIZE);

    line->dirty = 0;
}

/*
 * If an L1 cache line is dirty, write it into L2 before a replacement.
 */
/*
 * Obtain a line for an L1 cache. The returned bytes are copied into data.
 */
static int l1_get_line(int proc_id,
                       int isFetch,
                       int line_no,
                       uint8_t *data)
{
    if (!valid_proc(proc_id) || data == NULL ||
        line_no < 0 || line_no >= MEMSIZE / CACHE_LINE_SIZE)
        return -1;

    CacheLine *cache_array;
    int lines;
    unsigned long *hits;
    unsigned long *misses;

    if (isFetch) {
        cache_array = l1_instruction[proc_id];
        lines = L1_INSTRUCTION_LINES;
        hits = &l1i_hits[proc_id];
        misses = &l1i_misses[proc_id];
    } else {
        cache_array = l1_data[proc_id];
        lines = L1_DATA_LINES;
        hits = &l1d_hits[proc_id];
        misses = &l1d_misses[proc_id];
    }

    int index = cache_index(line_no, lines);
    int tag = cache_tag(line_no, lines);
    CacheLine *line = &cache_array[index];

    if (line->valid && line->tag == tag) {
        (*hits)++;
        memcpy(data, line->data, CACHE_LINE_SIZE);
        return 0;
    }

    (*misses)++;

    /* First obtain the requested line from L2/L3/RAM. */
    uint8_t fetched[CACHE_LINE_SIZE];
    if (l2_get_line(proc_id, line_no, fetched) != 0)
        return -1;

    /* Then write back the L1 victim, if dirty. */
    if (line->valid)
        writeback_l1_line(proc_id, line, index, lines);

    line->valid = 1;
    line->dirty = 0;
    line->tag = tag;
    memcpy(line->data, fetched, CACHE_LINE_SIZE);
    memcpy(data, fetched, CACHE_LINE_SIZE);
    return 0;
}

/*
 * Invalidate another core's data copy of a line after a data write.
 * Dirty data is written back before invalidation so that it cannot be lost.
 */
static void invalidate_other_l1_data_copies(int owner_proc, int line_no)
{
    for (int p = 0; p < NP; p++) {
        if (p == owner_proc)
            continue;

        int index = cache_index(line_no, L1_DATA_LINES);
        CacheLine *line = &l1_data[p][index];

        if (line->valid &&
            line->tag == cache_tag(line_no, L1_DATA_LINES)) {
            if (line->dirty)
                writeback_l1_line(p, line, index, L1_DATA_LINES);
            clear_line(line);
        }
    }
}

/*
 * Invalidate instruction copies on all cores for a data line that has just
 * been modified. This prevents stale instruction/data aliases.
 */
static void invalidate_all_l1_instruction_copies(int line_no)
{
    for (int p = 0; p < NP; p++) {
        int index = cache_index(line_no, L1_INSTRUCTION_LINES);
        CacheLine *line = &l1_instruction[p][index];

        if (line->valid &&
            line->tag == cache_tag(line_no, L1_INSTRUCTION_LINES)) {
            if (line->dirty)
                writeback_l1_line(p, line, index, L1_INSTRUCTION_LINES);
            clear_line(line);
        }
    }
}

void cache_initialize(void)
{
    for (int p = 0; p < NP; p++) {
        initialize_lines(l1_instruction[p], L1_INSTRUCTION_LINES);
        initialize_lines(l1_data[p], L1_DATA_LINES);
        initialize_lines(l2[p], L2_LINES);
    }

    initialize_lines(l3, L3_LINES);

    memset(l1i_hits, 0, sizeof(l1i_hits));
    memset(l1i_misses, 0, sizeof(l1i_misses));
    memset(l1d_hits, 0, sizeof(l1d_hits));
    memset(l1d_misses, 0, sizeof(l1d_misses));
    memset(l2_hits, 0, sizeof(l2_hits));
    memset(l2_misses, 0, sizeof(l2_misses));
    l3_hits = 0;
    l3_misses = 0;
}

void cache_finalize(void)
{
    /* Preserve dirty data all the way to RAM before resetting the hierarchy. */
    cache_flush(-1);
    cache_initialize();
}

int cache_read(int proc_id, int isFetch, int physical_address, uint8_t *value)
{
    if (!valid_proc(proc_id) || !valid_address(physical_address) || value == NULL)
        return -1;

    int line_no = line_number(physical_address);
    int offset = line_offset(physical_address);
    uint8_t data[CACHE_LINE_SIZE];

    if (l1_get_line(proc_id, isFetch, line_no, data) != 0)
        return -1;

    /* Re-read from the L1 copy so a replacement cannot affect the result. */
    int lines = isFetch ? L1_INSTRUCTION_LINES : L1_DATA_LINES;
    CacheLine *array = isFetch
        ? l1_instruction[proc_id]
        : l1_data[proc_id];
    int index = cache_index(line_no, lines);

    if (!array[index].valid ||
        array[index].tag != cache_tag(line_no, lines))
        return -1;

    *value = array[index].data[offset];
    return 0;
}

int cache_write(int proc_id, int physical_address, uint8_t value)
{
    if (!valid_proc(proc_id) || !valid_address(physical_address))
        return -1;

    int line_no = line_number(physical_address);
    int offset = line_offset(physical_address);
    int index = cache_index(line_no, L1_DATA_LINES);
    CacheLine *line = &l1_data[proc_id][index];

    /* Write-allocate through the normal L1-D -> L2 -> L3 path. */
    if (!line->valid || line->tag != cache_tag(line_no, L1_DATA_LINES)) {
        uint8_t ignored[CACHE_LINE_SIZE];
        if (l1_get_line(proc_id, 0, line_no, ignored) != 0)
            return -1;
        line = &l1_data[proc_id][index];
    }

    /* Keep other L1 copies from returning stale data. */
    invalidate_other_l1_data_copies(proc_id, line_no);
    invalidate_all_l1_instruction_copies(line_no);

    line->data[offset] = value;
    line->dirty = 1;
    return 0;
}

/*
 * Drain dirty lines associated with a physical frame:
 *
 *     L1-I/L1-D -> L2 -> L3 -> RAM
 *
 * Clean copies are invalidated as well because the frame may be reused by a
 * different logical page immediately after this operation.
 */
void cache_writeback_frame(int physical_frame)
{
    if (physical_frame <= 0 || physical_frame >= NUM_PHYSICAL_PAGES)
        return;

    int frame_start = physical_frame * PAGESIZE;
    int frame_end = frame_start + PAGESIZE;

    /* First drain every matching dirty L1 line into its private L2. */
    for (int p = 0; p < NP; p++) {
        for (int i = 0; i < L1_INSTRUCTION_LINES; i++) {
            CacheLine *line = &l1_instruction[p][i];
            if (!line->valid)
                continue;

            int line_no = cached_line_number(line, i, L1_INSTRUCTION_LINES);
            int start = line_base_from_number(line_no);
            if (start >= frame_start && start < frame_end && line->dirty)
                writeback_l1_line(p, line, i, L1_INSTRUCTION_LINES);
        }

        for (int i = 0; i < L1_DATA_LINES; i++) {
            CacheLine *line = &l1_data[p][i];
            if (!line->valid)
                continue;

            int line_no = cached_line_number(line, i, L1_DATA_LINES);
            int start = line_base_from_number(line_no);
            if (start >= frame_start && start < frame_end && line->dirty)
                writeback_l1_line(p, line, i, L1_DATA_LINES);
        }
    }

    /* Then drain every matching dirty L2 line into the shared L3. */
    for (int p = 0; p < NP; p++) {
        for (int i = 0; i < L2_LINES; i++) {
            CacheLine *line = &l2[p][i];
            if (!line->valid)
                continue;

            int line_no = cached_line_number(line, i, L2_LINES);
            int start = line_base_from_number(line_no);
            if (start >= frame_start && start < frame_end && line->dirty)
                writeback_l2_line(p, i);
        }
    }

    /* Finally drain matching dirty L3 lines into physical RAM. */
    for (int i = 0; i < L3_LINES; i++) {
        CacheLine *line = &l3[i];
        if (!line->valid)
            continue;

        int line_no = cached_line_number(line, i, L3_LINES);
        int start = line_base_from_number(line_no);
        if (start >= frame_start && start < frame_end && line->dirty)
            writeback_l3_line(i);
    }
}

void cache_invalidate_frame(int physical_frame)
{
    if (physical_frame <= 0 || physical_frame >= NUM_PHYSICAL_PAGES)
        return;

    int frame_start = physical_frame * PAGESIZE;
    int frame_end = frame_start + PAGESIZE;

    /* Dirty data must reach RAM before any matching line is discarded. */
    cache_writeback_frame(physical_frame);

    for (int p = 0; p < NP; p++) {
        for (int i = 0; i < L1_INSTRUCTION_LINES; i++) {
            CacheLine *line = &l1_instruction[p][i];
            if (!line->valid)
                continue;
            int line_no = cached_line_number(line, i, L1_INSTRUCTION_LINES);
            int start = line_base_from_number(line_no);
            if (start >= frame_start && start < frame_end)
                clear_line(line);
        }

        for (int i = 0; i < L1_DATA_LINES; i++) {
            CacheLine *line = &l1_data[p][i];
            if (!line->valid)
                continue;
            int line_no = cached_line_number(line, i, L1_DATA_LINES);
            int start = line_base_from_number(line_no);
            if (start >= frame_start && start < frame_end)
                clear_line(line);
        }

        for (int i = 0; i < L2_LINES; i++) {
            CacheLine *line = &l2[p][i];
            if (!line->valid)
                continue;
            int line_no = cached_line_number(line, i, L2_LINES);
            int start = line_base_from_number(line_no);
            if (start >= frame_start && start < frame_end)
                clear_line(line);
        }
    }

    for (int i = 0; i < L3_LINES; i++) {
        CacheLine *line = &l3[i];
        if (!line->valid)
            continue;
        int line_no = cached_line_number(line, i, L3_LINES);
        int start = line_base_from_number(line_no);
        if (start >= frame_start && start < frame_end)
            clear_line(line);
    }
}

/*
 * Flush one core's L1 caches into its private L2, preserving the L2 contents.
 * A global flush drains L1 -> L2 -> L3 -> RAM.
 */
void cache_flush(int proc_id)
{
    if (proc_id == -1) {
        /* L1 -> private L2. */
        for (int p = 0; p < NP; p++) {
            for (int i = 0; i < L1_INSTRUCTION_LINES; i++) {
                CacheLine *line = &l1_instruction[p][i];
                if (line->valid && line->dirty)
                    writeback_l1_line(p, line, i, L1_INSTRUCTION_LINES);
            }

            for (int i = 0; i < L1_DATA_LINES; i++) {
                CacheLine *line = &l1_data[p][i];
                if (line->valid && line->dirty)
                    writeback_l1_line(p, line, i, L1_DATA_LINES);
            }
        }

        /* L2 -> shared L3. */
        for (int p = 0; p < NP; p++) {
            for (int i = 0; i < L2_LINES; i++) {
                if (l2[p][i].valid && l2[p][i].dirty)
                    writeback_l2_line(p, i);
            }
        }

        /* L3 -> physical RAM. */
        for (int i = 0; i < L3_LINES; i++) {
            if (l3[i].valid && l3[i].dirty)
                writeback_l3_line(i);
        }

        return;
    }

    if (!valid_proc(proc_id))
        return;

    for (int i = 0; i < L1_INSTRUCTION_LINES; i++) {
        CacheLine *line = &l1_instruction[proc_id][i];
        if (line->valid && line->dirty)
            writeback_l1_line(proc_id, line, i, L1_INSTRUCTION_LINES);
    }

    for (int i = 0; i < L1_DATA_LINES; i++) {
        CacheLine *line = &l1_data[proc_id][i];
        if (line->valid && line->dirty)
            writeback_l1_line(proc_id, line, i, L1_DATA_LINES);
    }
}

static double hit_rate(unsigned long hits, unsigned long misses)
{
    unsigned long accesses = hits + misses;
    return accesses
        ? 100.0 * (double)hits / (double)accesses
        : 0.0;
}

void cache_print_stats(void)
{
    unsigned long total_l1i_hits = 0;
    unsigned long total_l1i_misses = 0;
    unsigned long total_l1d_hits = 0;
    unsigned long total_l1d_misses = 0;
    unsigned long total_l2_hits = 0;
    unsigned long total_l2_misses = 0;

    printf("\n========================================\n");
    printf(" Cache Statistics\n");
    printf("========================================\n\n");

    printf("L1 Instruction Cache: 128 B/core\n");
    printf("----------------------------------------\n");
    for (int p = 0; p < NP; p++) {
        printf("Core %d: hits=%lu misses=%lu hit-rate=%.2f%%\n",
               p,
               l1i_hits[p],
               l1i_misses[p],
               hit_rate(l1i_hits[p], l1i_misses[p]));
        total_l1i_hits += l1i_hits[p];
        total_l1i_misses += l1i_misses[p];
    }
    printf("Total: hits=%lu misses=%lu hit-rate=%.2f%%\n\n",
           total_l1i_hits,
           total_l1i_misses,
           hit_rate(total_l1i_hits, total_l1i_misses));

    printf("L1 Data Cache: 128 B/core\n");
    printf("----------------------------------------\n");
    for (int p = 0; p < NP; p++) {
        printf("Core %d: hits=%lu misses=%lu hit-rate=%.2f%%\n",
               p,
               l1d_hits[p],
               l1d_misses[p],
               hit_rate(l1d_hits[p], l1d_misses[p]));
        total_l1d_hits += l1d_hits[p];
        total_l1d_misses += l1d_misses[p];
    }
    printf("Total: hits=%lu misses=%lu hit-rate=%.2f%%\n\n",
           total_l1d_hits,
           total_l1d_misses,
           hit_rate(total_l1d_hits, total_l1d_misses));

    printf("Private L2 Cache: 256 B/core\n");
    printf("----------------------------------------\n");
    for (int p = 0; p < NP; p++) {
        printf("Core %d: hits=%lu misses=%lu hit-rate=%.2f%%\n",
               p,
               l2_hits[p],
               l2_misses[p],
               hit_rate(l2_hits[p], l2_misses[p]));
        total_l2_hits += l2_hits[p];
        total_l2_misses += l2_misses[p];
    }
    printf("Total: hits=%lu misses=%lu hit-rate=%.2f%%\n\n",
           total_l2_hits,
           total_l2_misses,
           hit_rate(total_l2_hits, total_l2_misses));

    printf("Shared Unified L3 Cache: 512 B\n");
    printf("----------------------------------------\n");
    printf("Hits   : %lu\n", l3_hits);
    printf("Misses : %lu\n", l3_misses);
    printf("Hit-rate: %.2f%%\n", hit_rate(l3_hits, l3_misses));
    printf("========================================\n");
}
