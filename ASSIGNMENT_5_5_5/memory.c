#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "processor.h"
#include "tlb.h"


/*
 * ============================================================
 * Physical memory
 * ============================================================
 */

uint8_t memory[MEMSIZE];


/*
 * ============================================================
 * Page tables
 * ============================================================
 *
 * pageTable[processor][logical_page]
 *
 * -1 = page not mapped
 * >=0 = physical frame
 */

int pageTable[NP][NUM_LOGICAL_PAGES];


/*
 * ============================================================
 * Physical frame allocation table
 * ============================================================
 *
 * 0 = free
 * 1 = allocated/reserved
 *
 * Frame 0 is permanently reserved.
 */

int freePages[NUM_PHYSICAL_PAGES];


/*
 * ============================================================
 * Internal helper
 * ============================================================
 */

static void clear_page_table(int proc_id)
{
    int page;

    if (proc_id < 0 || proc_id >= NP) {
        return;
    }

    for (page = 0; page < NUM_LOGICAL_PAGES; page++) {
        pageTable[proc_id][page] = -1;
    }
}


/*
 * ============================================================
 * initialize_physical_memory
 * ============================================================
 */

static void initialize_physical_memory(void)
{
    int i;

    /*
     * Clear physical memory.
     */
    memset(memory, 0, sizeof(memory));


    /*
     * Mark every physical frame as free.
     */
    for (i = 0; i < NUM_PHYSICAL_PAGES; i++) {
        freePages[i] = 0;
    }


    /*
     * Frame 0 is reserved by the operating system.
     */
    freePages[0] = 1;
}


/*
 * ============================================================
 * initialize_memory
 * ============================================================
 */

void initialize_memory(void)
{
    int proc_id;

    /*
     * Initialize physical memory and frame allocation.
     */
    initialize_physical_memory();


    /*
     * Clear all process page tables.
     */
    for (proc_id = 0; proc_id < NP; proc_id++) {
        clear_page_table(proc_id);
    }


    /*
     * Initialize the TLB.
     */
    initialize_tlb();
}


/*
 * ============================================================
 * finalize_memory
 * ============================================================
 */

void finalize_memory(void)
{
    /*
     * The TLB does not own physical memory, but it must be
     * invalidated before the memory subsystem shuts down.
     */
    finalize_tlb();
}


/*
 * ============================================================
 * load_hex_file
 * ============================================================
 *
 * Loads hexadecimal bytes from a text file.
 *
 * The file is expected to contain hexadecimal byte values.
 *
 * Returns:
 *
 *      number of bytes loaded
 *      -1 on failure
 * ============================================================
 */

static int load_hex_file(const char *filename,
                         uint8_t *buffer,
                         int max_size)
{
    FILE *file;
    unsigned int value;
    int count = 0;

    if (filename == NULL ||
        buffer == NULL ||
        max_size <= 0) {
        return -1;
    }

    file = fopen(filename, "r");

    if (file == NULL) {
        return -1;
    }

    while (count < max_size &&
           fscanf(file, "%x", &value) == 1) {

        if (value > 0xFF) {
            fclose(file);
            return -1;
        }

        buffer[count++] = (uint8_t)value;
    }

    fclose(file);

    return count;
}


/*
 * ============================================================
 * pages_required
 * ============================================================
 */

static int pages_required(int bytes)
{
    if (bytes <= 0) {
        return 0;
    }

    return (bytes + PAGESIZE - 1) / PAGESIZE;
}


/*
 * ============================================================
 * getFreePage
 * ============================================================
 */

int getFreePage(void)
{
    int frame;

    /*
     * Start at frame 1 because frame 0 is reserved.
     */
    for (frame = 1; frame < NUM_PHYSICAL_PAGES; frame++) {

        if (freePages[frame] == 0) {

            freePages[frame] = 1;

            /*
             * Clear the newly allocated frame.
             */
            memset(&memory[frame * PAGESIZE],
                   0,
                   PAGESIZE);

            return frame;
        }
    }

    return -1;
}


/*
 * ============================================================
 * freePage
 * ============================================================
 */

void freePage(int frame)
{
    if (frame <= 0 ||
        frame >= NUM_PHYSICAL_PAGES) {
        return;
    }

    freePages[frame] = 0;

    /*
     * Clear the released physical frame.
     */
    memset(&memory[frame * PAGESIZE],
           0,
           PAGESIZE);
}


/*
 * ============================================================
 * getPhysicallAddress
 * ============================================================
 *
 * Logical address -> physical address.
 *
 * Translation path:
 *
 *     logical address
 *          |
 *          v
 *         TLB
 *       /     \
 *    HIT       MISS
 *     |          |
 *     |      page table
 *     |          |
 *     |          v
 *     |      physical frame
 *     |          |
 *     |       TLB insert
 *     |          |
 *     +----------+
 *          |
 *          v
 *    physical address
 *
 * isFetch:
 *
 *      1 = instruction access
 *      0 = data access
 * ============================================================
 */

int getPhysicallAddress(int proc_id,
                        int isFetch,
                        int address)
{
    int logical_page;
    int offset;
    int physical_frame;
    int physical_address;


    /*
     * --------------------------------------------------------
     * Validate processor ID.
     * --------------------------------------------------------
     */

    if (proc_id < 0 ||
        proc_id >= NP) {
        return -1;
    }


    /*
     * --------------------------------------------------------
     * Determine logical page and offset.
     * --------------------------------------------------------
     */

    if (isFetch) {

        /*
         * Instruction memory:
         *
         * logical pages 0-1
         */
        if (address < 0 ||
            address >= INSTRUCTION_SIZE) {
            return -1;
        }

        logical_page = address / PAGESIZE;
        offset = address % PAGESIZE;

    } else {

        /*
         * Data memory:
         *
         * logical pages 2-9
         */
        if (address < 0 ||
            address >= DATA_SIZE) {
            return -1;
        }

        logical_page =
            address / PAGESIZE +
            (INSTRUCTION_SIZE / PAGESIZE);

        offset = address % PAGESIZE;
    }


    /*
     * --------------------------------------------------------
     * TLB LOOKUP
     * --------------------------------------------------------
     */

    physical_frame =
        tlb_lookup(proc_id, logical_page);


    /*
     * --------------------------------------------------------
     * TLB MISS
     * --------------------------------------------------------
     */

    if (physical_frame < 0) {

        /*
         * TLB miss.
         *
         * Consult the authoritative page table.
         */
        physical_frame =
            pageTable[proc_id][logical_page];


        /*
         * Page is not currently mapped.
         *
         * Demand paging can be added here later.
         */
        if (physical_frame < 0 ||
            physical_frame >= NUM_PHYSICAL_PAGES ||
            physical_frame == 0) {

            return -1;
        }


        /*
         * Page-table translation succeeded.
         *
         * Populate the TLB.
         */
        tlb_insert(proc_id,
                   logical_page,
                   physical_frame);
    }


    /*
     * --------------------------------------------------------
     * Construct physical address.
     * --------------------------------------------------------
     */

    physical_address =
        physical_frame * PAGESIZE + offset;


    /*
     * Final physical-memory bounds check.
     */
    if (physical_address < 0 ||
        physical_address >= MEMSIZE) {

        return -1;
    }

    return physical_address;
}


/*
 * ============================================================
 * allocate_instruction_pages
 * ============================================================
 */

static int allocate_instruction_pages(int proc_id,
                                      int instruction_bytes)
{
    int required;
    int page;
    int frame;

    required = pages_required(instruction_bytes);

    if (required > INSTRUCTION_SIZE / PAGESIZE) {
        return -1;
    }

    for (page = 0; page < required; page++) {

        frame = getFreePage();

        if (frame < 0) {

            /*
             * Roll back pages already allocated.
             */
            int rollback;

            for (rollback = 0;
                 rollback < page;
                 rollback++) {

                if (pageTable[proc_id][rollback] >= 0) {

                    freePage(
                        pageTable[proc_id][rollback]
                    );

                    pageTable[proc_id][rollback] = -1;
                }
            }

            return -1;
        }

        pageTable[proc_id][page] = frame;
    }

    return 0;
}


/*
 * ============================================================
 * allocate_data_pages
 * ============================================================
 */

static int allocate_data_pages(int proc_id,
                               int data_bytes)
{
    int required;
    int page;
    int frame;
    int logical_page;

    required = pages_required(data_bytes);

    if (required > DATA_SIZE / PAGESIZE) {
        return -1;
    }

    for (page = 0; page < required; page++) {

        frame = getFreePage();

        if (frame < 0) {

            /*
             * Roll back data pages already allocated.
             */
            int rollback;

            for (rollback = 0;
                 rollback < page;
                 rollback++) {

                logical_page =
                    (INSTRUCTION_SIZE / PAGESIZE) +
                    rollback;

                if (pageTable[proc_id][logical_page] >= 0) {

                    freePage(
                        pageTable[proc_id][logical_page]
                    );

                    pageTable[proc_id][logical_page] = -1;
                }
            }

            return -1;
        }

        logical_page =
            (INSTRUCTION_SIZE / PAGESIZE) + page;

        pageTable[proc_id][logical_page] = frame;
    }

    return 0;
}


/*
 * ============================================================
 * copy_instruction_bytes
 * ============================================================
 */

static int copy_instruction_bytes(int proc_id,
                                  const uint8_t *buffer,
                                  int byte_count)
{
    int i;
    int logical_page;
    int offset;
    int frame;
    int physical_address;

    if (proc_id < 0 ||
        proc_id >= NP ||
        buffer == NULL ||
        byte_count < 0 ||
        byte_count > INSTRUCTION_SIZE) {
        return -1;
    }

    for (i = 0; i < byte_count; i++) {

        logical_page = i / PAGESIZE;
        offset = i % PAGESIZE;

        frame = pageTable[proc_id][logical_page];

        if (frame < 1 ||
            frame >= NUM_PHYSICAL_PAGES) {
            return -1;
        }

        physical_address =
            frame * PAGESIZE + offset;

        memory[physical_address] = buffer[i];
    }

    return 0;
}


/*
 * ============================================================
 * copy_data_bytes
 * ============================================================
 */

static int copy_data_bytes(int proc_id,
                           const uint8_t *buffer,
                           int byte_count)
{
    int i;
    int logical_page;
    int offset;
    int frame;
    int physical_address;

    if (proc_id < 0 ||
        proc_id >= NP ||
        buffer == NULL ||
        byte_count < 0 ||
        byte_count > DATA_SIZE) {
        return -1;
    }

    for (i = 0; i < byte_count; i++) {

        logical_page =
            i / PAGESIZE +
            (INSTRUCTION_SIZE / PAGESIZE);

        offset = i % PAGESIZE;

        frame = pageTable[proc_id][logical_page];

        if (frame < 1 ||
            frame >= NUM_PHYSICAL_PAGES) {
            return -1;
        }

        physical_address =
            frame * PAGESIZE + offset;

        memory[physical_address] = buffer[i];
    }

    return 0;
}


/*
 * ============================================================
 * load_process_memory
 * ============================================================
 */

int load_process_memory(int proc_id,
                        const char *program_file,
                        const char *data_file)
{
    uint8_t instruction_buffer[INSTRUCTION_SIZE];
    uint8_t data_buffer[DATA_SIZE];

    int instruction_bytes;
    int data_bytes = 0;
    int i;


    /*
     * --------------------------------------------------------
     * Validate arguments.
     * --------------------------------------------------------
     */

    if (proc_id < 0 ||
        proc_id >= NP ||
        program_file == NULL) {
        return -1;
    }


    /*
     * --------------------------------------------------------
     * Release any previous memory belonging to this process.
     * --------------------------------------------------------
     */

    release_process_memory(proc_id);


    /*
     * --------------------------------------------------------
     * Load instruction image.
     * --------------------------------------------------------
     */

    memset(instruction_buffer,
           0,
           sizeof(instruction_buffer));

    instruction_bytes =
        load_hex_file(program_file,
                      instruction_buffer,
                      INSTRUCTION_SIZE);

    if (instruction_bytes < 0) {
        return -1;
    }


    /*
     * --------------------------------------------------------
     * Allocate instruction pages.
     * --------------------------------------------------------
     */

    if (allocate_instruction_pages(proc_id,
                                   instruction_bytes) < 0) {

        release_process_memory(proc_id);

        return -1;
    }


    /*
     * --------------------------------------------------------
     * Copy instruction image into physical memory.
     * --------------------------------------------------------
     */

    if (copy_instruction_bytes(proc_id,
                               instruction_buffer,
                               instruction_bytes) < 0) {

        release_process_memory(proc_id);

        return -1;
    }


    /*
     * --------------------------------------------------------
     * Load optional data image.
     * --------------------------------------------------------
 */

    if (data_file != NULL) {

        memset(data_buffer,
               0,
               sizeof(data_buffer));

        data_bytes =
            load_hex_file(data_file,
                          data_buffer,
                          DATA_SIZE);

        if (data_bytes < 0) {

            release_process_memory(proc_id);

            return -1;
        }


        /*
         * Remove trailing zero bytes.
         *
         * This preserves the behavior of the original memory
         * implementation where the data image is sized to the
         * actual non-zero contents.
         */
        while (data_bytes > 0 &&
               data_buffer[data_bytes - 1] == 0) {

            data_bytes--;
        }
    }


    /*
     * --------------------------------------------------------
     * Allocate data pages.
     * --------------------------------------------------------
 */

    if (data_bytes > 0) {

        if (allocate_data_pages(proc_id,
                                data_bytes) < 0) {

            release_process_memory(proc_id);

            return -1;
        }


        /*
         * Copy data into physical memory.
         */
        if (copy_data_bytes(proc_id,
                            data_buffer,
                            data_bytes) < 0) {

            release_process_memory(proc_id);

            return -1;
        }
    }


    /*
     * --------------------------------------------------------
     * Print memory mapping summary.
     * --------------------------------------------------------
 */

    printf("\nProcess %d memory loaded.\n", proc_id);

    printf("Instruction bytes: %d\n",
           instruction_bytes);

    printf("Data bytes:        %d\n",
           data_bytes);

    printf("Page table:\n");

    for (i = 0; i < NUM_LOGICAL_PAGES; i++) {

        if (pageTable[proc_id][i] >= 0) {

            printf("  Logical page %d -> "
                   "Physical frame %d\n",
                   i,
                   pageTable[proc_id][i]);

        } else {

            printf("  Logical page %d -> "
                   "unmapped\n",
                   i);
        }
    }

    return 0;
}


/*
 * ============================================================
 * unload_process_memory
 * ============================================================
 */

void unload_process_memory(int proc_id)
{
    release_process_memory(proc_id);
}


/*
 * ============================================================
 * save_data_file
 * ============================================================
 *
 * The current TLB implementation does not change the
 * representation of process data, so this routine writes
 * logical data bytes through the existing page table.
 *
 * Returns:
 *
 *      0  = success
 *     -1  = failure
 * ============================================================
 */

static int save_data_file(int proc_id,
                          const char *filename)
{
    FILE *file;
    int logical_address;
    int physical_address;
    int logical_page;
    int offset;
    int frame;

    if (proc_id < 0 ||
        proc_id >= NP ||
        filename == NULL) {
        return -1;
    }

    file = fopen(filename, "w");

    if (file == NULL) {
        return -1;
    }


    /*
     * Write the complete logical data region.
     *
     * Unmapped pages are treated as zero-filled.
     */
    for (logical_address = 0;
         logical_address < DATA_SIZE;
         logical_address++) {

        logical_page =
            logical_address / PAGESIZE +
            (INSTRUCTION_SIZE / PAGESIZE);

        offset =
            logical_address % PAGESIZE;

        frame =
            pageTable[proc_id][logical_page];

        if (frame < 1 ||
            frame >= NUM_PHYSICAL_PAGES) {

            fprintf(file, "00\n");
            continue;
        }

        physical_address =
            frame * PAGESIZE + offset;

        if (physical_address < 0 ||
            physical_address >= MEMSIZE) {

            fclose(file);
            return -1;
        }

        fprintf(file,
                "%02X\n",
                memory[physical_address]);
    }

    fclose(file);

    return 0;
}


/*
 * ============================================================
 * release_process_memory
 * ============================================================
 */

void release_process_memory(int proc_id)
{
    int page;
    int frame;

    if (proc_id < 0 ||
        proc_id >= NP) {
        return;
    }


    /*
     * --------------------------------------------------------
     * IMPORTANT:
     *
     * Invalidate all TLB entries BEFORE releasing the physical
     * frames.
     *
     * Otherwise the TLB could continue returning a physical
     * frame that has already been reassigned to another
     * process.
     * --------------------------------------------------------
     */

    tlb_flush_process(proc_id);


    /*
     * --------------------------------------------------------
     * Release all mapped physical frames.
     * --------------------------------------------------------
     */

    for (page = 0;
         page < NUM_LOGICAL_PAGES;
         page++) {

        frame = pageTable[proc_id][page];

        if (frame >= 1 &&
            frame < NUM_PHYSICAL_PAGES) {

            freePage(frame);

            pageTable[proc_id][page] = -1;
        }
    }
}


/*
 * ============================================================
 * Physical byte access
 * ============================================================
 */

int read_physical_byte(int physical_address,
                       uint8_t *value)
{
    if (value == NULL) {
        return -1;
    }

    if (physical_address < 0 ||
        physical_address >= MEMSIZE) {
        return -1;
    }

    *value = memory[physical_address];

    return 0;
}


/*
 * ============================================================
 * write_physical_byte
 * ============================================================
 */

int write_physical_byte(int physical_address,
                        uint8_t value)
{
    if (physical_address < 0 ||
        physical_address >= MEMSIZE) {
        return -1;
    }

    memory[physical_address] = value;

    return 0;
}


/*
 * ============================================================
 * Compatibility functions
 * ============================================================
 */

void initialize(void)
{
    initialize_memory();
}


void finalize(void)
{
    finalize_memory();
}

