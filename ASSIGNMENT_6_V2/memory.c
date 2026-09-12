#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "processor.h"
#include "cache.h"
#include "tlb.h"
#include "swap.h"


/*
 * ============================================================
 * PHYSICAL MEMORY
 * ============================================================
 *
 * Physical memory:
 *
 *     8192 bytes
 *
 * Page/frame size:
 *
 *     512 bytes
 *
 * Number of physical frames:
 *
 *     8192 / 512 = 16
 *
 * Frame 0 is reserved.
 * ============================================================
 */

uint8_t memory[MEMSIZE];


/*
 * ============================================================
 * MEMORY-RESIDENT PAGE TABLES (LAB 6)
 * ============================================================
 *
 * Frame 0 is reserved for page tables.  Each processor owns a
 * NUM_LOGICAL_PAGES-byte region in that frame, and PTBR[proc_id]
 * points to the first byte of the region.
 *
 * Page-table entries are bytes: 0 = unmapped, 1..15 = frame.
 * ============================================================
 */

_Static_assert(NP * NUM_LOGICAL_PAGES <= PAGESIZE,
               "page tables do not fit in reserved frame 0");


/*
 * ============================================================
 * PHYSICAL FRAME ALLOCATION TABLE
 * ============================================================
 *
 *     0 = free
 *     1 = allocated
 *
 * Frame 0 is permanently reserved.
 * ============================================================
 */

int freePages[NUM_PHYSICAL_PAGES];

/* Logical pages that belong to each process, whether resident or swapped. */
static uint8_t page_defined[NP][NUM_LOGICAL_PAGES];


/*
 * ============================================================
 * VALIDATION HELPERS
 * ============================================================
 */

static int valid_proc(int proc_id)
{
    return proc_id >= 0 && proc_id < NP;
}


static int valid_frame(int frame)
{
    return frame >= 0 &&
           frame < NUM_PHYSICAL_PAGES;
}


static int valid_logical_page(int page)
{
    return page >= 0 &&
           page < NUM_LOGICAL_PAGES;
}


/*
 * ============================================================
 * CLEAR ONE PROCESS PAGE TABLE
 * ============================================================
 */

static void clear_page_table(int proc_id)
{
    if (!valid_proc(proc_id))
        return;

    int base = proc_id * NUM_LOGICAL_PAGES;

    for (int page = 0;
         page < NUM_LOGICAL_PAGES;
         page++) {

        memory[base + page] = 0;
    }
}


/*
 * ============================================================
 * INITIALIZE PHYSICAL MEMORY
 * ============================================================
 */

static void initialize_physical_memory(void)
{
    /*
     * Clear all physical memory.
     */
    memset(memory,
           0,
           sizeof(memory));


    /*
     * Initially all frames are free.
     */
    for (int frame = 0;
         frame < NUM_PHYSICAL_PAGES;
         frame++) {

        freePages[frame] = 0;
    }


    /*
     * Frame 0 is reserved by the OS.
     */
    freePages[0] = 1;


    memset(page_defined, 0, sizeof(page_defined));

    /*
     * Clear every process page table.
     */
    for (int proc = 0;
         proc < NP;
         proc++) {

        PTBR[proc] = proc * NUM_LOGICAL_PAGES;
        clear_page_table(proc);
    }
}


/*
 * ============================================================
 * GLOBAL MEMORY INITIALIZATION
 * ============================================================
 *
 * Called by os.c:
 *
 *     initialize_memory();
 * ============================================================
 */

void initialize_memory(void)
{
    initialize_physical_memory();
}


/*
 * ============================================================
 * LOAD HEXADECIMAL BYTE FILE
 * ============================================================
 *
 * Reads hexadecimal byte values from a file.
 *
 * Example:
 *
 *     FF 01 0A 20
 *
 * Each value must be between 00 and FF.
 *
 * max_bytes provides a hard upper bound, so this routine
 * cannot continue indefinitely even if the input file is
 * unexpectedly large.
 * ============================================================
 */

static int load_hex_file(const char *filename,
                         uint8_t *destination,
                         int max_bytes,
                         int *bytes_loaded)
{
    if (filename == NULL)
        return -1;

    if (destination == NULL)
        return -1;

    if (bytes_loaded == NULL)
        return -1;

    if (max_bytes < 0)
        return -1;


    FILE *fp = fopen(filename, "r");

    if (fp == NULL)
        return -1;


    int count = 0;
    unsigned int value;


    /*
     * Maximum iterations = max_bytes.
     */
    while (count < max_bytes) {

        int result =
            fscanf(fp, "%x", &value);


        /*
         * EOF or malformed input.
         */
        if (result != 1)
            break;


        /*
         * A byte cannot exceed FF.
         */
        if (value > 0xFF) {

            fclose(fp);

            return -1;
        }


        destination[count] =
            (uint8_t)value;

        count++;
    }


    /*
     * If fscanf stopped before EOF, determine whether the
     * remaining character is merely whitespace or malformed
     * input.
     */
    if (!feof(fp)) {

        int c;


        /*
         * This loop consumes whitespace only.
         * A finite input file guarantees termination.
         */
        do {
            c = fgetc(fp);
        }
        while (c == ' ' ||
               c == '\t' ||
               c == '\n' ||
               c == '\r');


        /*
         * Anything other than EOF means malformed input.
         */
        if (c != EOF) {

            fclose(fp);

            return -1;
        }
    }


    fclose(fp);


    *bytes_loaded = count;

    return 0;
}


/*
 * ============================================================
 * CALCULATE REQUIRED NUMBER OF PAGES
 * ============================================================
 *
 * Returns:
 *
 *     ceil(bytes / PAGESIZE)
 *
 * Examples:
 *
 *     1 byte   -> 1 page
 *     512      -> 1 page
 *     513      -> 2 pages
 * ============================================================
 */

static int pages_required(int bytes)
{
    if (bytes <= 0)
        return 0;

    return (bytes + PAGESIZE - 1) / PAGESIZE;
}


/*
 * ============================================================
 * GET FREE PHYSICAL PAGE
 * ============================================================
 *
 * Searches physical frames 1 through 15.
 *
 * Frame 0 is reserved.
 *
 * Maximum number of iterations:
 *
 *     15
 *
 * Therefore this function cannot enter an infinite loop.
 * ============================================================
 */

int getFreePage(void)
{
    for (int frame = 1;
         frame < NUM_PHYSICAL_PAGES;
         frame++) {

        if (freePages[frame] == 0) {

            /*
             * Mark frame as allocated.
             */
            freePages[frame] = 1;


            /*
             * Clear the frame before giving it to a process.
             */
            memset(&memory[frame * PAGESIZE],
                   0,
                   PAGESIZE);


            return frame;
        }
    }


    /*
     * All usable frames are occupied.
     */
    fprintf(stderr,
            "ERROR: no free physical page available\n");


    return -1;
}


/*
 * ============================================================
 * FREE PHYSICAL PAGE
 * ============================================================
 */

void freePage(int frame)
{
    /*
     * Frame 0 is reserved.
     */
    if (frame <= 0)
        return;


    if (frame >= NUM_PHYSICAL_PAGES)
        return;


    /*
     * Already-free frame.
     */
    if (freePages[frame] == 0)
        return;


    /*
     * Push dirty cache data to RAM and invalidate every cache/TLB
     * reference before the frame can be reused.
     */
    cache_invalidate_frame(frame);
    tlb_invalidate_frame(frame);

    /*
     * Mark the frame free.
     */
    freePages[frame] = 0;


    /*
     * Clear its contents.
     */
    memset(&memory[frame * PAGESIZE],
           0,
           PAGESIZE);
}


/*
 * ============================================================
 * LOGICAL -> PHYSICAL ADDRESS TRANSLATION
 * ============================================================
 *
 * Instruction access:
 *
 *     logical_page = address / PAGESIZE
 *
 *
 * Data access:
 *
 *     logical_page =
 *         address / PAGESIZE
 *         + INSTRUCTION_SIZE / PAGESIZE
 *
 *
 * Physical address:
 *
 *     frame * PAGESIZE + offset
 *
 * ============================================================
 */

/*
 * Find which process/logical page currently owns a resident frame.
 * Page-table contents remain authoritative; this metadata is only
 * used to locate a victim for paging.
 */
static int find_frame_owner(int frame, int *owner_proc, int *owner_page)
{
    if (frame <= 0 || frame >= NUM_PHYSICAL_PAGES ||
        owner_proc == NULL || owner_page == NULL)
        return -1;

    for (int p = 0; p < NP; p++) {
        if (PTBR[p] < 0 ||
            PTBR[p] + NUM_LOGICAL_PAGES > MEMSIZE)
            continue;

        for (int page = 0; page < NUM_LOGICAL_PAGES; page++) {
            if (!page_defined[p][page])
                continue;

            if ((int)memory[PTBR[p] + page] == frame) {
                *owner_proc = p;
                *owner_page = page;
                return 0;
            }
        }
    }

    return -1;
}

/*
 * Evict one resident logical page to swap.  The requesting page is
 * never selected as its own victim.  The caller supplies whether
 * pages belonging to the requesting process may be evicted.
 */
static int evict_one_page(int requester_proc,
                          int requester_page,
                          int allow_same_proc)
{
    /*
     * Prefer data pages over instruction pages. This keeps the hot
     * instruction working set resident while processes contend for
     * the smaller data-memory portion of physical RAM.
     */
    for (int pass = 0; pass < 4; pass++) {
        for (int frame = 1; frame < NUM_PHYSICAL_PAGES; frame++) {
            int owner_proc = -1;
            int owner_page = -1;

            if (find_frame_owner(frame, &owner_proc, &owner_page) != 0)
                continue;

            if (owner_proc == requester_proc && owner_page == requester_page)
                continue;

            int same_proc = (owner_proc == requester_proc);
            int is_data = (owner_page >= INSTRUCTION_SIZE / PAGESIZE);

            /* Pass 0: other-process data. */
            if (pass == 0 && (same_proc || !is_data))
                continue;

            /* Pass 1: requesting-process data, when permitted. */
            if (pass == 1 && (!same_proc || !is_data || !allow_same_proc))
                continue;

            /* Pass 2: other-process instruction pages. */
            if (pass == 2 && (same_proc || is_data))
                continue;

            /* Pass 3: requesting-process instruction pages, when permitted. */
            if (pass == 3 && (!same_proc || is_data || !allow_same_proc))
                continue;

            cache_writeback_frame(frame);

            uint8_t page_buffer[PAGESIZE];
            memcpy(page_buffer,
                   &memory[frame * PAGESIZE],
                   PAGESIZE);

            if (swap_write_page(owner_proc,
                                owner_page,
                                page_buffer) != 0)
                return -1;

            memory[PTBR[owner_proc] + owner_page] = 0;
            tlb_invalidate(owner_proc, owner_page);
            freePage(frame);

            fprintf(stderr,
                    "[MEM] page-out proc=%d page=%d frame=%d\n",
                    owner_proc,
                    owner_page,
                    frame);
            return 0;
        }
    }

    return -1;
}

static int allocate_frame_for_page(int proc_id,
                                   int logical_page,
                                   int allow_same_proc)
{
    int frame = getFreePage();
    if (frame >= 0)
        return frame;

    if (evict_one_page(proc_id,
                       logical_page,
                       allow_same_proc) != 0)
        return -1;

    return getFreePage();
}

/* Bring a previously defined, swapped-out logical page back into RAM. */
static int page_in(int proc_id, int logical_page)
{
    if (!valid_proc(proc_id) || !valid_logical_page(logical_page))
        return -1;

    if (!page_defined[proc_id][logical_page])
        return -1;

    int frame = allocate_frame_for_page(proc_id, logical_page, 1);
    if (frame < 0)
        return -1;

    uint8_t page_buffer[PAGESIZE];
    memset(page_buffer, 0, sizeof(page_buffer));

    if (swap_has_page(proc_id, logical_page)) {
        if (swap_read_page(proc_id,
                           logical_page,
                           page_buffer) != 0) {
            freePage(frame);
            return -1;
        }
    }

    memcpy(&memory[frame * PAGESIZE], page_buffer, PAGESIZE);
    memory[PTBR[proc_id] + logical_page] = (uint8_t)frame;
    tlb_insert(proc_id, logical_page, frame);

    fprintf(stderr,
            "[MEM] page-in proc=%d page=%d frame=%d\n",
            proc_id,
            logical_page,
            frame);
    return frame;
}

int getPhysicallAddress(int proc_id,
                        int isFetch,
                        int address)
{
    /*
     * Validate process.
     */
    if (!valid_proc(proc_id))
        return -1;


    /*
     * Negative logical addresses are invalid.
     */
    if (address < 0)
        return -1;


    int logical_page;
    int offset;


    /*
     * --------------------------------------------------------
     * INSTRUCTION ACCESS
     * --------------------------------------------------------
     *
     * Instruction memory:
     *
     *     0 .. 1023
     */
    if (isFetch) {

        if (address >= INSTRUCTION_SIZE)
            return -1;


        logical_page =
            address / PAGESIZE;


        offset =
            address % PAGESIZE;
    }


    /*
     * --------------------------------------------------------
     * DATA ACCESS
     * --------------------------------------------------------
     *
     * Data memory:
     *
     *     0 .. 4095
     *
     * Page-table indices begin at page 2 because instruction
     * memory occupies two pages.
     */
    else {

        if (address >= DATA_SIZE)
            return -1;


        logical_page =
            address / PAGESIZE
            + INSTRUCTION_SIZE / PAGESIZE;


        offset =
            address % PAGESIZE;
    }


    /*
     * Validate logical page number.
     */
    if (!valid_logical_page(logical_page))
        return -1;


    /*
     * Look up the physical frame in the task's memory-resident
     * page table using its page-table base register.
     */
    if (PTBR[proc_id] < 0 ||
        PTBR[proc_id] + NUM_LOGICAL_PAGES > MEMSIZE)
        return -1;

    /* Fast path: consult the per-processor TLB first. */
    int physical_frame = tlb_lookup(proc_id, logical_page);

    if (physical_frame > 0 &&
        valid_frame(physical_frame) &&
        freePages[physical_frame] != 0) {
        /* TLB hit. */
    } else {
        /* TLB miss: consult the memory-resident page table. */
        physical_frame =
            (int)memory[PTBR[proc_id] + logical_page];

        if (physical_frame <= 0 ||
            !valid_frame(physical_frame) ||
            freePages[physical_frame] == 0) {
            /* A defined but non-resident page may be paged in. */
            if (!page_defined[proc_id][logical_page])
                return -1;

            physical_frame = page_in(proc_id, logical_page);
            if (physical_frame < 0)
                return -1;
        } else {
            tlb_insert(proc_id, logical_page, physical_frame);
        }
    }


    /*
     * Calculate physical address.
     */
    int physical_address =
        physical_frame * PAGESIZE + offset;


    /*
     * Final physical-memory bounds check.
     */
    if (physical_address < 0 ||
        physical_address >= MEMSIZE)
        return -1;


    return physical_address;
}


/*
 * ============================================================
 * ALLOCATE INSTRUCTION PAGES
 * ============================================================
 */

static int allocate_instruction_pages(int proc_id,
                                      int bytes)
{
    if (!valid_proc(proc_id))
        return -1;


    int required =
        pages_required(bytes);


    /*
     * Instruction memory contains only two pages.
     */
    if (required > INSTRUCTION_SIZE / PAGESIZE)
        return -1;


    /*
     * At most two iterations.
     */
    for (int page = 0;
         page < required;
         page++) {

        int frame =
            allocate_frame_for_page(proc_id, page, 0);


        if (frame < 0) {

            /*
             * Roll back pages allocated so far.
             */
            release_process_memory(proc_id);

            return -1;
        }


        memory[PTBR[proc_id] + page] =
            (uint8_t)frame;
        page_defined[proc_id][page] = 1;
        tlb_insert(proc_id, page, frame);
    }


    return 0;
}


/*
 * ============================================================
 * ALLOCATE DATA PAGES
 * ============================================================
 */

static int allocate_data_pages(int proc_id,
                               int bytes)
{
    if (!valid_proc(proc_id))
        return -1;


    int required =
        pages_required(bytes);


    /*
     * Data memory contains eight pages.
     */
    if (required > DATA_SIZE / PAGESIZE)
        return -1;


    /*
     * Logical data pages begin at page 2.
     */
    int first_data_page =
        INSTRUCTION_SIZE / PAGESIZE;


    /*
     * Maximum number of iterations = 8.
     */
    for (int page = 0;
         page < required;
         page++) {

        int frame =
            allocate_frame_for_page(proc_id,
                                    first_data_page + page,
                                    0);


        if (frame < 0) {

            /*
             * Roll back all pages allocated for this process.
             */
            release_process_memory(proc_id);

            return -1;
        }


        memory[PTBR[proc_id] + first_data_page + page] =
            (uint8_t)frame;
        page_defined[proc_id][first_data_page + page] = 1;
        tlb_insert(proc_id, first_data_page + page, frame);
    }


    return 0;
}


/*
 * ============================================================
 * COPY INSTRUCTION BYTES INTO PHYSICAL MEMORY
 * ============================================================
 */

static int copy_instruction_bytes(int proc_id,
                                  const uint8_t *bytes,
                                  int count)
{
    if (!valid_proc(proc_id))
        return -1;


    if (bytes == NULL)
        return -1;


    if (count < 0 ||
        count > INSTRUCTION_SIZE)
        return -1;


    /*
     * Maximum iterations = 1024.
     */
    for (int address = 0;
         address < count;
         address++) {

        int physical =
            getPhysicallAddress(proc_id,
                                1,
                                address);


        if (physical < 0)
            return -1;


        memory[physical] =
            bytes[address];
    }


    return 0;
}


/*
 * ============================================================
 * COPY DATA BYTES INTO PHYSICAL MEMORY
 * ============================================================
 */

static int copy_data_bytes(int proc_id,
                           const uint8_t *bytes,
                           int count)
{
    if (!valid_proc(proc_id))
        return -1;


    if (bytes == NULL)
        return -1;


    if (count < 0 ||
        count > DATA_SIZE)
        return -1;


    /*
     * Maximum iterations = 4096.
     */
    for (int address = 0;
         address < count;
         address++) {

        int physical =
            getPhysicallAddress(proc_id,
                                0,
                                address);


        if (physical < 0)
            return -1;


        memory[physical] =
            bytes[address];
    }


    return 0;
}


/*
 * ============================================================
 * LOAD PROCESS MEMORY
 * ============================================================
 *
 * This is the process-level loader used by os.c.
 *
 * It:
 *
 *     1. Clears any previous mapping
 *     2. Loads program.byte
 *     3. Allocates instruction pages
 *     4. Loads data.byte if present
 *     5. Allocates data pages
 *     6. Copies everything into physical memory
 *
 * Returns:
 *
 *     0  = success
 *    -1  = failure
 * ============================================================
 */

int load_process_memory(int proc_id,
                        const char *program_file,
                        const char *data_file)
{
    if (!valid_proc(proc_id))
        return -1;


    if (program_file == NULL)
        return -1;


    /*
     * Make sure this process starts with no old mapping.
     */
    release_process_memory(proc_id);

    /*
     * Assign this processor's page-table region in reserved
     * physical Frame 0 before allocating any process pages.
     */
    PTBR[proc_id] = proc_id * NUM_LOGICAL_PAGES;
    memset(page_defined[proc_id], 0, sizeof(page_defined[proc_id]));
    tlb_flush(proc_id);
    clear_page_table(proc_id);


    /*
     * Temporary logical images.
     *
     * They are bounded by the architecture's logical memory
     * sizes.
     */
    uint8_t instruction_bytes[INSTRUCTION_SIZE];

    uint8_t data_bytes[DATA_SIZE];


    memset(instruction_bytes,
           0,
           sizeof(instruction_bytes));


    memset(data_bytes,
           0,
           sizeof(data_bytes));


    int instruction_count = 0;
    int data_count = 0;


    /*
     * --------------------------------------------------------
     * LOAD PROGRAM
     * --------------------------------------------------------
     */

    if (load_hex_file(program_file,
                      instruction_bytes,
                      INSTRUCTION_SIZE,
                      &instruction_count) != 0) {

        fprintf(stderr,
                "ERROR: cannot load program file %s\n",
                program_file);

        return -1;
    }


    /*
     * Program must fit inside 1024-byte instruction memory.
     */
    if (instruction_count > INSTRUCTION_SIZE) {

        fprintf(stderr,
                "ERROR: program exceeds instruction memory\n");

        return -1;
    }


    /*
     * Determine number of instruction pages.
     */
    int instruction_pages =
        pages_required(instruction_count);


    /*
     * Keep one instruction page available for an empty/minimal
     * program image.
     */
    if (instruction_pages == 0)
        instruction_pages = 1;


    /*
     * Allocate the instruction pages.
     */
    if (allocate_instruction_pages(proc_id,
                                   instruction_count) != 0) {

        fprintf(stderr,
                "ERROR: unable to allocate instruction pages\n");

        release_process_memory(proc_id);

        return -1;
    }


    /*
     * Copy program into physical memory.
     */
    if (copy_instruction_bytes(proc_id,
                               instruction_bytes,
                               instruction_count) != 0) {

        fprintf(stderr,
                "ERROR: failed to copy program into memory\n");

        release_process_memory(proc_id);

        return -1;
    }


    /*
     * --------------------------------------------------------
     * LOAD DATA
     * --------------------------------------------------------
     *
     * data.byte may be absent.
     */

    if (data_file != NULL &&
        data_file[0] != '\0') {

        if (load_hex_file(data_file,
                          data_bytes,
                          DATA_SIZE,
                          &data_count) != 0) {

            fprintf(stderr,
                    "ERROR: cannot load data file %s\n",
                    data_file);

            release_process_memory(proc_id);

            return -1;
        }
    }
    
    /*
 * Trim trailing zero bytes.
 *
 * save_data_file() writes the entire logical DATA_SIZE address
 * space, including unmapped/unused locations as zero. Those
 * trailing zeroes should not force allocation of all eight
 * logical data pages when the file is loaded again.
 */
	while (data_count > 0 &&
       		data_bytes[data_count - 1] == 0) {
    			data_count--;
	}


    /*
     * No data pages are required if data.byte is empty.
     */
    printf("[MEM] proc=%d: instruction=%d bytes (%d pages), "
       "data=%d bytes (%d pages)\n",
       proc_id,
       instruction_count,
       pages_required(instruction_count),
       data_count,
       pages_required(data_count));

    if (data_count > 0) {

        /*
         * Allocate physical pages for data.
         */
        if (allocate_data_pages(proc_id,
                                data_count) != 0) {

            fprintf(stderr,
                    "ERROR: unable to allocate data pages\n");

            release_process_memory(proc_id);

            return -1;
        }


        /*
         * Copy data into physical memory.
         */
        if (copy_data_bytes(proc_id,
                            data_bytes,
                            data_count) != 0) {

            fprintf(stderr,
                    "ERROR: failed to copy data into memory\n");

            release_process_memory(proc_id);

            return -1;
        }
    }


    return 0;
}


/*
 * ============================================================
 * UNLOAD PROCESS MEMORY
 * ============================================================
 *
 * This is the API currently expected by os.c.
 * ============================================================
 */

void unload_process_memory(int proc_id)
{
    if (!valid_proc(proc_id))
        return;


    release_process_memory(proc_id);
}


/*
 * ============================================================
 * RELEASE PROCESS MEMORY
 * ============================================================
 */

void release_process_memory(int proc_id)
{
    if (!valid_proc(proc_id))
        return;


    /*
     * Exactly NUM_LOGICAL_PAGES entries are examined.
     */
    if (PTBR[proc_id] < 0 ||
        PTBR[proc_id] + NUM_LOGICAL_PAGES > MEMSIZE) {
        PTBR[proc_id] = -1;
        return;
    }

    for (int page = 0;
         page < NUM_LOGICAL_PAGES;
         page++) {

        int frame =
            (int)memory[PTBR[proc_id] + page];

        if (frame > 0 &&
            frame < NUM_PHYSICAL_PAGES) {
            freePage(frame);
        }

        /* Release any disk backing belonging to this logical page. */
        (void)swap_free(proc_id, page);
        tlb_invalidate(proc_id, page);
        page_defined[proc_id][page] = 0;

        /* Remove the mapping from physical memory. */
        memory[PTBR[proc_id] + page] = 0;
    }

    tlb_flush(proc_id);
    PTBR[proc_id] = -1;
}


/*
 * ============================================================
 * GLOBAL MEMORY FINALIZATION
 * ============================================================
 *
 * Called by os.c:
 *
 *     finalize_memory();
 * ============================================================
 */

void finalize_memory(void)
{
    /*
     * Release every process.
     *
     * NP is finite.
     */
    for (int proc = 0;
         proc < NP;
         proc++) {

        release_process_memory(proc);
    }


    /*
     * Clear physical memory.
     */
    memset(memory,
           0,
           sizeof(memory));


    /*
     * Reset frame allocation table.
     */
    for (int frame = 0;
         frame < NUM_PHYSICAL_PAGES;
         frame++) {

        freePages[frame] = 0;
    }


    /*
     * Frame 0 remains reserved.
     */
    freePages[0] = 1;
}


/*
 * ============================================================
 * READ PHYSICAL BYTE
 * ============================================================
 */

int read_physical_byte(int physical_address,
                       uint8_t *value)
{
    if (value == NULL)
        return -1;


    if (physical_address < 0 ||
        physical_address >= MEMSIZE)
        return -1;


    *value =
        memory[physical_address];


    return 0;
}


/*
 * ============================================================
 * WRITE PHYSICAL BYTE
 * ============================================================
 */

int write_physical_byte(int physical_address,
                        uint8_t value)
{
    if (physical_address < 0 ||
        physical_address >= MEMSIZE)
        return -1;


    memory[physical_address] =
        value;


    return 0;
}


/*
 * ============================================================
 * COMPATIBILITY INITIALIZE
 * ============================================================
 *
 * Retained for older starter-code references.
 * ============================================================
 */

void initialize(void)
{
    initialize_memory();
}


/*
 * ============================================================
 * COMPATIBILITY FINALIZE
 * ============================================================
 *
 * Retained for older starter-code references.
 * ============================================================
 */

void finalize(void)
{
    finalize_memory();
}
