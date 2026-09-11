#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "cache.h"
#include "memory.h"
#include "processor.h"

/*
 * ================================================================
 * Cache hierarchy
 * ================================================================
 *
 * Per processor:
 *
 *      L1 Instruction Cache = 128 B
 *      L1 Data Cache        = 128 B
 *
 * Shared:
 *
 *      Unified L2 Cache     = 512 B
 *
 * Cache line size = 16 B
 *
 * Therefore:
 *
 *      L1-I = 8 lines/core
 *      L1-D = 8 lines/core
 *      L2   = 32 lines
 *
 * All caches are direct mapped.
 *
 * L1-I : write-back / write-allocate
 * L1-D : write-back / write-allocate
 * L2   : write-back / write-allocate
 *
 * Physical memory is the backing store for L2.
 *
 * Paging path:
 *
 *      L1 -> L2 -> memory[] -> swap -> disk.img
 *
 * The cache does NOT access disk directly.
 */


/* ================================================================
 * Cache line
 * ================================================================ */

typedef struct
{
    int valid;
    int dirty;
    int tag;

    uint8_t data[CACHE_LINE_SIZE];

} CacheLine;


/* ================================================================
 * Cache storage
 * ================================================================ */

/* Private L1 instruction cache for every processor. */
static CacheLine l1_instruction[NP][L1_INSTRUCTION_LINES];

/* Private L1 data cache for every processor. */
static CacheLine l1_data[NP][L1_DATA_LINES];

/* Unified shared L2 cache. */
static CacheLine l2[L2_LINES];


/* ================================================================
 * Statistics
 * ================================================================ */

static unsigned long l1i_hits[NP];
static unsigned long l1i_misses[NP];

static unsigned long l1d_hits[NP];
static unsigned long l1d_misses[NP];

static unsigned long l2_hits;
static unsigned long l2_misses;


/* ================================================================
 * Address helpers
 * ================================================================ */

static int line_number(int physical_address)
{
    return physical_address / CACHE_LINE_SIZE;
}


static int line_offset(int physical_address)
{
    return physical_address % CACHE_LINE_SIZE;
}


static int line_base(int physical_address)
{
    return physical_address -
           line_offset(physical_address);
}


static int cache_tag(int physical_address)
{
    return line_number(physical_address);
}


static int l1i_index(int physical_address)
{
    return line_number(physical_address) %
           L1_INSTRUCTION_LINES;
}


static int l1d_index(int physical_address)
{
    return line_number(physical_address) %
           L1_DATA_LINES;
}


static int l2_index(int physical_address)
{
    return line_number(physical_address) %
           L2_LINES;
}


/* ================================================================
 * Forward declarations
 * ================================================================ */

static void writeback_l2_line(CacheLine *line);

static void evict_l2_line(CacheLine *line);

static void writeback_l1_line(int proc_id,
                              CacheLine *line);

static CacheLine *l2_get_line(int physical_address);

static CacheLine *l1_get_line(int proc_id,
                              int isFetch,
                              int physical_address);


/* ================================================================
 * L2 write-back
 * ================================================================ */

/*
 * Write a dirty L2 line into physical memory.
 *
 * This is the final cache-level write-back.
 */
static void writeback_l2_line(CacheLine *line)
{
    int i;
    int base;

    if (!line->valid || !line->dirty)
        return;

    base = line->tag * CACHE_LINE_SIZE;

    if (base < 0 ||
        base + CACHE_LINE_SIZE > MEMSIZE)
    {
        line->dirty = 0;
        return;
    }

    for (i = 0; i < CACHE_LINE_SIZE; i++)
        memory[base + i] = line->data[i];

    line->dirty = 0;
}


/* ================================================================
 * L1 -> L2 write-back
 * ================================================================ */

/*
 * Push a dirty L1 line into the shared L2.
 *
 * IMPORTANT:
 *
 * If the corresponding L2 slot already contains the same tag,
 * this is simply an update.
 *
 * If the L2 slot contains a different tag, the old L2 line must
 * be evicted first.
 */
static void writeback_l1_line(int proc_id,
                              CacheLine *line)
{
    int i;
    int base;
    int index;

    CacheLine *l2_line;

    (void)proc_id;

    if (!line->valid || !line->dirty)
        return;

    base = line->tag * CACHE_LINE_SIZE;

    if (base < 0 ||
        base + CACHE_LINE_SIZE > MEMSIZE)
    {
        line->dirty = 0;
        return;
    }

    index = l2_index(base);
    l2_line = &l2[index];

    /*
     * Same physical line already exists in L2.
     *
     * Do NOT evict it.
     */
    if (!l2_line->valid ||
        l2_line->tag == line->tag)
    {
        l2_line->valid = 1;
        l2_line->dirty = 1;
        l2_line->tag = line->tag;

        for (i = 0; i < CACHE_LINE_SIZE; i++)
            l2_line->data[i] = line->data[i];

        line->dirty = 0;

        return;
    }

    /*
     * Different line occupies the required L2 slot.
     */
    evict_l2_line(l2_line);

    l2_line->valid = 1;
    l2_line->dirty = 1;
    l2_line->tag = line->tag;

    for (i = 0; i < CACHE_LINE_SIZE; i++)
        l2_line->data[i] = line->data[i];

    line->dirty = 0;
}


/* ================================================================
 * L2 eviction
 * ================================================================ */

/*
 * Evict one L2 line.
 *
 * Before the line reaches memory, check all L1 caches for a
 * dirty copy of the same physical cache line.
 */
static void evict_l2_line(CacheLine *line)
{
    int p;
    int i;
    int base;

    if (!line->valid)
        return;

    /*
     * Find dirty L1-I copies.
     */
    for (p = 0; p < NP; p++)
    {
        CacheLine *i_line =
            &l1_instruction[p]
                [l1i_index(line->tag * CACHE_LINE_SIZE)];

        if (i_line->valid &&
            i_line->tag == line->tag &&
            i_line->dirty)
        {
            for (i = 0; i < CACHE_LINE_SIZE; i++)
                line->data[i] = i_line->data[i];

            line->dirty = 1;
            i_line->dirty = 0;
        }
    }

    /*
     * Find dirty L1-D copies.
     */
    for (p = 0; p < NP; p++)
    {
        CacheLine *d_line =
            &l1_data[p]
                [l1d_index(line->tag * CACHE_LINE_SIZE)];

        if (d_line->valid &&
            d_line->tag == line->tag &&
            d_line->dirty)
        {
            for (i = 0; i < CACHE_LINE_SIZE; i++)
                line->data[i] = d_line->data[i];

            line->dirty = 1;
            d_line->dirty = 0;
        }
    }

    /*
     * Write the final L2 contents to physical memory.
     */
    if (line->dirty)
    {
        base = line->tag * CACHE_LINE_SIZE;

        if (base >= 0 &&
            base + CACHE_LINE_SIZE <= MEMSIZE)
        {
            for (i = 0; i < CACHE_LINE_SIZE; i++)
                memory[base + i] = line->data[i];
        }
    }

    line->valid = 0;
    line->dirty = 0;
    line->tag = 0;
}


/* ================================================================
 * Dirty L1 snooping
 * ================================================================ */

/*
 * Make dirty L1 copies of ONE physical cache line visible in L2.
 *
 * This function deliberately does NOT scan unrelated L1 lines.
 *
 * It is called only after the L2 line lookup has been performed.
 *
 * This ordering is important:
 *
 *      WRONG:
 *
 *          dirty L1 write-back
 *                  |
 *                  v
 *          possibly evict L2 line
 *                  |
 *                  v
 *          check L2
 *
 *
 *      CORRECT:
 *
 *          check L2 first
 *                  |
 *          +-------+-------+
 *          |               |
 *        HIT             MISS
 *          |               |
 *          v               v
 *      synchronize     synchronize
 *          |               |
 *          +-------+-------+
 *                  |
 *                  v
 *              continue
 */
static void writeback_dirty_l1_copy(int physical_address)
{
    int p;
    int tag;
    int base;

    tag = cache_tag(physical_address);
    base = line_base(physical_address);

    for (p = 0; p < NP; p++)
    {
        CacheLine *i_line =
            &l1_instruction[p][l1i_index(base)];

        if (i_line->valid &&
            i_line->tag == tag &&
            i_line->dirty)
        {
            writeback_l1_line(p, i_line);
        }

        {
            CacheLine *d_line =
                &l1_data[p][l1d_index(base)];

            if (d_line->valid &&
                d_line->tag == tag &&
                d_line->dirty)
            {
                writeback_l1_line(p, d_line);
            }
        }
    }
}


/* ================================================================
 * L2 lookup / fill
 * ================================================================ */

static CacheLine *l2_get_line(int physical_address)
{
    int i;
    int base;
    int tag;
    int index;

    CacheLine *line;

    base = line_base(physical_address);
    tag = cache_tag(physical_address);
    index = l2_index(physical_address);

    line = &l2[index];

    /*
     * ------------------------------------------------------------
     * FIRST check for an L2 hit.
     * ------------------------------------------------------------
     *
     * This is the critical correction.
     *
     * The previous implementation performed dirty-L1 write-back
     * before this check. A dirty L1 line mapping to the same L2
     * index could therefore evict the requested L2 line and turn
     * a genuine L2 hit into a miss.
     */
    if (line->valid &&
        line->tag == tag)
    {
        l2_hits++;

        /*
         * If an L1 cache still has a dirty copy of this exact
         * physical line, synchronize it into L2.
         *
         * Because the L2 tag is the SAME tag, this operation
         * updates the existing L2 line rather than evicting it.
         */
        writeback_dirty_l1_copy(physical_address);

        return line;
    }

    /*
     * Genuine L2 miss.
     */
    l2_misses++;

    /*
     * Before replacing this L2 slot, first preserve any dirty
     * L1 copy of the requested physical line.
     *
     * This matters when the requested line exists dirty in L1
     * but was previously evicted from L2.
     */
    writeback_dirty_l1_copy(physical_address);

    /*
     * Re-evaluate the L2 slot.
     *
     * A dirty L1 write-back may have filled the exact requested
     * line into L2.
     */
    if (line->valid &&
        line->tag == tag)
    {
        return line;
    }

    /*
     * The slot is still occupied by another line.
     */
    if (line->valid)
        evict_l2_line(line);

    /*
     * Fill the new L2 line from physical memory.
     */
    line->valid = 1;
    line->dirty = 0;
    line->tag = tag;

    if (base >= 0 &&
        base + CACHE_LINE_SIZE <= MEMSIZE)
    {
        for (i = 0; i < CACHE_LINE_SIZE; i++)
            line->data[i] = memory[base + i];
    }
    else
    {
        memset(line->data,
               0,
               CACHE_LINE_SIZE);
    }

    return line;
}


/* ================================================================
 * L1 lookup / fill
 * ================================================================ */

static CacheLine *l1_get_line(int proc_id,
                              int isFetch,
                              int physical_address)
{
    int i;
    int tag;
    int index;

    CacheLine *line;
    CacheLine *l2_line;

    tag = cache_tag(physical_address);

    /*
     * Select the appropriate private L1 cache.
     */
    if (isFetch)
    {
        index = l1i_index(physical_address);

        line = &l1_instruction[proc_id][index];
    }
    else
    {
        index = l1d_index(physical_address);

        line = &l1_data[proc_id][index];
    }

    /*
     * This function is normally called after the caller has
     * determined that the L1 lookup missed, but keep this check
     * for correctness.
     */
    if (line->valid &&
        line->tag == tag)
    {
        return line;
    }

    /*
     * Write back dirty L1 victim.
     */
    if (line->valid &&
        line->dirty)
    {
        writeback_l1_line(proc_id, line);
    }

    /*
     * Fetch the requested cache line from shared L2.
     */
    l2_line = l2_get_line(physical_address);

    line->valid = 1;
    line->dirty = 0;
    line->tag = tag;

    for (i = 0; i < CACHE_LINE_SIZE; i++)
        line->data[i] = l2_line->data[i];

    return line;
}


/* ================================================================
 * Initialization
 * ================================================================ */

void cache_initialize(void)
{
    int p;

    memset(l1_instruction,
           0,
           sizeof(l1_instruction));

    memset(l1_data,
           0,
           sizeof(l1_data));

    memset(l2,
           0,
           sizeof(l2));

    for (p = 0; p < NP; p++)
    {
        l1i_hits[p] = 0;
        l1i_misses[p] = 0;

        l1d_hits[p] = 0;
        l1d_misses[p] = 0;
    }

    l2_hits = 0;
    l2_misses = 0;
}


/* ================================================================
 * Read
 * ================================================================ */

int cache_read(int proc_id,
               int isFetch,
               int physical_address,
               uint8_t *value)
{
    CacheLine *line;
    int tag;
    int offset;

    if (value == NULL)
        return -1;

    if (proc_id < 0 ||
        proc_id >= NP)
    {
        return -1;
    }

    if (physical_address < 0 ||
        physical_address >= MEMSIZE)
    {
        return -1;
    }

    tag = cache_tag(physical_address);
    offset = line_offset(physical_address);

    /*
     * ------------------------------------------------------------
     * Instruction fetch
     * ------------------------------------------------------------
     */
    if (isFetch)
    {
        line =
            &l1_instruction[proc_id]
                          [l1i_index(physical_address)];

        if (line->valid &&
            line->tag == tag)
        {
            l1i_hits[proc_id]++;
        }
        else
        {
            l1i_misses[proc_id]++;

            line =
                l1_get_line(proc_id,
                            1,
                            physical_address);
        }
    }

    /*
     * ------------------------------------------------------------
     * Data read
     * ------------------------------------------------------------
     */
    else
    {
        line =
            &l1_data[proc_id]
                    [l1d_index(physical_address)];

        if (line->valid &&
            line->tag == tag)
        {
            l1d_hits[proc_id]++;
        }
        else
        {
            l1d_misses[proc_id]++;

            line =
                l1_get_line(proc_id,
                            0,
                            physical_address);
        }
    }

    *value = line->data[offset];

    return 0;
}


/* ================================================================
 * Write
 * ================================================================ */

int cache_write(int proc_id,
                int physical_address,
                uint8_t value)
{
    int p;
    int tag;
    int offset;

    CacheLine *line;

    if (proc_id < 0 ||
        proc_id >= NP)
    {
        return -1;
    }

    if (physical_address < 0 ||
        physical_address >= MEMSIZE)
    {
        return -1;
    }

    tag = cache_tag(physical_address);
    offset = line_offset(physical_address);

    /*
     * Data writes always go through L1-D.
     */
    line =
        &l1_data[proc_id]
                [l1d_index(physical_address)];

    /*
     * L1-D hit.
     */
    if (line->valid &&
        line->tag == tag)
    {
        l1d_hits[proc_id]++;
    }

    /*
     * L1-D miss.
     *
     * Write-allocate: fetch the complete cache line first.
     */
    else
    {
        l1d_misses[proc_id]++;

        line =
            l1_get_line(proc_id,
                        0,
                        physical_address);
    }

    /*
     * Perform the actual store.
     *
     * Physical memory is intentionally NOT updated here.
     *
     * This is write-back behavior.
     */
    line->data[offset] = value;
    line->dirty = 1;


    /*
     * ------------------------------------------------------------
     * Coherence:
     *
     * Invalidate copies in other processors' L1-D caches.
     * ------------------------------------------------------------
     */
    for (p = 0; p < NP; p++)
    {
        CacheLine *other;

        if (p == proc_id)
            continue;

        other =
            &l1_data[p]
                    [l1d_index(physical_address)];

        if (other->valid &&
            other->tag == tag)
        {
            /*
             * Preserve dirty data before invalidation.
             */
            if (other->dirty)
                writeback_l1_line(p, other);

            other->valid = 0;
            other->dirty = 0;
        }
    }


    /*
     * ------------------------------------------------------------
     * Instruction/data aliasing.
     *
     * If a data write modifies a physical line that exists in an
     * L1 instruction cache, invalidate the instruction copy.
     * ------------------------------------------------------------
     */
    for (p = 0; p < NP; p++)
    {
        CacheLine *instruction_line;

        instruction_line =
            &l1_instruction[p]
                           [l1i_index(physical_address)];

        if (instruction_line->valid &&
            instruction_line->tag == tag)
        {
            /*
             * Normally L1-I should be clean, but preserve it
             * correctly if it is dirty.
             */
            if (instruction_line->dirty)
                writeback_l1_line(p,
                                  instruction_line);

            instruction_line->valid = 0;
            instruction_line->dirty = 0;
        }
    }

    return 0;
}


/* ================================================================
 * Write back one physical frame
 * ================================================================ */

/*
 * Ensure that every dirty cache line belonging to a physical page
 * reaches memory[].
 *
 * This function is essential before:
 *
 *      page_out()
 *      freePage()
 *      frame reuse
 *      swap_write_page()
 *
 * because swap.c reads from memory[].
 */
void cache_writeback_frame(int physical_frame)
{
    int p;
    int i;

    int frame_start;
    int frame_end;
    int base;

    if (physical_frame < 0 ||
        physical_frame >= NUM_PHYSICAL_PAGES)
    {
        return;
    }

    frame_start =
        physical_frame * PAGESIZE;

    frame_end =
        frame_start + PAGESIZE;


    /*
     * ------------------------------------------------------------
     * Step 1:
     *
     * Drain dirty L1-I lines into L2.
     * ------------------------------------------------------------
     */
    for (p = 0; p < NP; p++)
    {
        for (i = 0;
             i < L1_INSTRUCTION_LINES;
             i++)
        {
            CacheLine *line =
                &l1_instruction[p][i];

            if (!line->valid ||
                !line->dirty)
            {
                continue;
            }

            base =
                line->tag * CACHE_LINE_SIZE;

            if (base >= frame_start &&
                base < frame_end)
            {
                writeback_l1_line(p, line);
            }
        }
    }


    /*
     * ------------------------------------------------------------
     * Step 2:
     *
     * Drain dirty L1-D lines into L2.
     * ------------------------------------------------------------
     */
    for (p = 0; p < NP; p++)
    {
        for (i = 0;
             i < L1_DATA_LINES;
             i++)
        {
            CacheLine *line =
                &l1_data[p][i];

            if (!line->valid ||
                !line->dirty)
            {
                continue;
            }

            base =
                line->tag * CACHE_LINE_SIZE;

            if (base >= frame_start &&
                base < frame_end)
            {
                writeback_l1_line(p, line);
            }
        }
    }


    /*
     * ------------------------------------------------------------
     * Step 3:
     *
     * Drain matching dirty L2 lines into RAM.
     * ------------------------------------------------------------
     */
    for (i = 0;
         i < L2_LINES;
         i++)
    {
        CacheLine *line = &l2[i];

        if (!line->valid ||
            !line->dirty)
        {
            continue;
        }

        base =
            line->tag * CACHE_LINE_SIZE;

        if (base >= frame_start &&
            base < frame_end)
        {
            writeback_l2_line(line);
        }
    }
}


/* ================================================================
 * Invalidate one physical frame
 * ================================================================ */

void cache_invalidate_frame(int physical_frame)
{
    int p;
    int i;

    int frame_start;
    int frame_end;
    int base;

    if (physical_frame < 0 ||
        physical_frame >= NUM_PHYSICAL_PAGES)
    {
        return;
    }

    frame_start =
        physical_frame * PAGESIZE;

    frame_end =
        frame_start + PAGESIZE;


    /*
     * First make sure all dirty data reaches RAM.
     */
    cache_writeback_frame(physical_frame);


    /*
     * Invalidate L1-I lines.
     */
    for (p = 0; p < NP; p++)
    {
        for (i = 0;
             i < L1_INSTRUCTION_LINES;
             i++)
        {
            CacheLine *line =
                &l1_instruction[p][i];

            if (!line->valid)
                continue;

            base =
                line->tag * CACHE_LINE_SIZE;

            if (base >= frame_start &&
                base < frame_end)
            {
                line->valid = 0;
                line->dirty = 0;
            }
        }
    }


    /*
     * Invalidate L1-D lines.
     */
    for (p = 0; p < NP; p++)
    {
        for (i = 0;
             i < L1_DATA_LINES;
             i++)
        {
            CacheLine *line =
                &l1_data[p][i];

            if (!line->valid)
                continue;

            base =
                line->tag * CACHE_LINE_SIZE;

            if (base >= frame_start &&
                base < frame_end)
            {
                line->valid = 0;
                line->dirty = 0;
            }
        }
    }


    /*
     * Invalidate L2 lines.
     */
    for (i = 0;
         i < L2_LINES;
         i++)
    {
        CacheLine *line = &l2[i];

        if (!line->valid)
            continue;

        base =
            line->tag * CACHE_LINE_SIZE;

        if (base >= frame_start &&
            base < frame_end)
        {
            line->valid = 0;
            line->dirty = 0;
        }
    }
}


/* ================================================================
 * Cache flush
 * ================================================================ */

void cache_flush(int proc_id)
{
    int p;
    int i;

    /*
     * ------------------------------------------------------------
     * Flush one processor's L1 caches.
     * ------------------------------------------------------------
     */
    if (proc_id >= 0 &&
        proc_id < NP)
    {
        /*
         * L1-I
         */
        for (i = 0;
             i < L1_INSTRUCTION_LINES;
             i++)
        {
            CacheLine *line =
                &l1_instruction[proc_id][i];

            if (line->valid &&
                line->dirty)
            {
                writeback_l1_line(proc_id,
                                  line);
            }

            line->valid = 0;
            line->dirty = 0;
        }


        /*
         * L1-D
         */
        for (i = 0;
             i < L1_DATA_LINES;
             i++)
        {
            CacheLine *line =
                &l1_data[proc_id][i];

            if (line->valid &&
                line->dirty)
            {
                writeback_l1_line(proc_id,
                                  line);
            }

            line->valid = 0;
            line->dirty = 0;
        }

        return;
    }


    /*
     * ------------------------------------------------------------
     * Flush entire hierarchy.
     * ------------------------------------------------------------
     */
    if (proc_id == -1)
    {
        /*
         * First flush all private L1 caches into L2.
         */
        for (p = 0; p < NP; p++)
        {
            for (i = 0;
                 i < L1_INSTRUCTION_LINES;
                 i++)
            {
                CacheLine *line =
                    &l1_instruction[p][i];

                if (line->valid &&
                    line->dirty)
                {
                    writeback_l1_line(p,
                                      line);
                }

                line->valid = 0;
                line->dirty = 0;
            }


            for (i = 0;
                 i < L1_DATA_LINES;
                 i++)
            {
                CacheLine *line =
                    &l1_data[p][i];

                if (line->valid &&
                    line->dirty)
                {
                    writeback_l1_line(p,
                                      line);
                }

                line->valid = 0;
                line->dirty = 0;
            }
        }


        /*
         * Then flush the shared L2 into physical RAM.
         */
        for (i = 0;
             i < L2_LINES;
             i++)
        {
            CacheLine *line = &l2[i];

            if (line->valid &&
                line->dirty)
            {
                writeback_l2_line(line);
            }

            line->valid = 0;
            line->dirty = 0;
        }


        /*
         * Reset statistics on a complete flush.
         */
        for (p = 0; p < NP; p++)
        {
            l1i_hits[p] = 0;
            l1i_misses[p] = 0;

            l1d_hits[p] = 0;
            l1d_misses[p] = 0;
        }

        l2_hits = 0;
        l2_misses = 0;
    }
}


/* ================================================================
 * Statistics
 * ================================================================ */

void cache_print_stats(void)
{
    int p;

    unsigned long total_l1i_hits = 0;
    unsigned long total_l1i_misses = 0;

    unsigned long total_l1d_hits = 0;
    unsigned long total_l1d_misses = 0;

    unsigned long total_l1i_accesses;
    unsigned long total_l1d_accesses;
    unsigned long total_l2_accesses;


    printf("\n");
    printf("========================================\n");
    printf(" Cache Statistics\n");
    printf("========================================\n");


    /*
     * ------------------------------------------------------------
     * L1 Instruction
     * ------------------------------------------------------------
     */
    printf("\n");
    printf("L1 Instruction Cache: 128 B/core\n");
    printf("----------------------------------------\n");

    for (p = 0; p < NP; p++)
    {
        unsigned long accesses =
            l1i_hits[p] +
            l1i_misses[p];

        double rate = 0.0;

        if (accesses != 0)
        {
            rate =
                100.0 *
                (double)l1i_hits[p] /
                (double)accesses;
        }

        printf("Core %d: hits=%lu misses=%lu "
               "hit-rate=%.2f%%\n",
               p,
               l1i_hits[p],
               l1i_misses[p],
               rate);

        total_l1i_hits += l1i_hits[p];
        total_l1i_misses += l1i_misses[p];
    }

    total_l1i_accesses =
        total_l1i_hits +
        total_l1i_misses;

    printf("Total: hits=%lu misses=%lu",
           total_l1i_hits,
           total_l1i_misses);

    if (total_l1i_accesses != 0)
    {
        printf(" hit-rate=%.2f%%",
               100.0 *
               (double)total_l1i_hits /
               (double)total_l1i_accesses);
    }

    printf("\n");


    /*
     * ------------------------------------------------------------
     * L1 Data
     * ------------------------------------------------------------
     */
    printf("\n");
    printf("L1 Data Cache: 128 B/core\n");
    printf("----------------------------------------\n");

    for (p = 0; p < NP; p++)
    {
        unsigned long accesses =
            l1d_hits[p] +
            l1d_misses[p];

        double rate = 0.0;

        if (accesses != 0)
        {
            rate =
                100.0 *
                (double)l1d_hits[p] /
                (double)accesses;
        }

        printf("Core %d: hits=%lu misses=%lu "
               "hit-rate=%.2f%%\n",
               p,
               l1d_hits[p],
               l1d_misses[p],
               rate);

        total_l1d_hits += l1d_hits[p];
        total_l1d_misses += l1d_misses[p];
    }

    total_l1d_accesses =
        total_l1d_hits +
        total_l1d_misses;

    printf("Total: hits=%lu misses=%lu",
           total_l1d_hits,
           total_l1d_misses);

    if (total_l1d_accesses != 0)
    {
        printf(" hit-rate=%.2f%%",
               100.0 *
               (double)total_l1d_hits /
               (double)total_l1d_accesses);
    }

    printf("\n");


    /*
     * ------------------------------------------------------------
     * Shared L2
     * ------------------------------------------------------------
     */
    printf("\n");
    printf("Unified L2 Cache: 512 B\n");
    printf("----------------------------------------\n");

    total_l2_accesses =
        l2_hits +
        l2_misses;

    printf("Hits   : %lu\n",
           l2_hits);

    printf("Misses : %lu\n",
           l2_misses);

    if (total_l2_accesses != 0)
    {
        printf("Hit-rate: %.2f%%\n",
               100.0 *
               (double)l2_hits /
               (double)total_l2_accesses);
    }
    else
    {
        printf("Hit-rate: 0.00%%\n");
    }

    printf("========================================\n");
}


/* ================================================================
 * Finalization
 * ================================================================ */

void cache_finalize(void)
{
    /*
     * Write every dirty cache line back to physical memory.
     */
    cache_flush(-1);
}
