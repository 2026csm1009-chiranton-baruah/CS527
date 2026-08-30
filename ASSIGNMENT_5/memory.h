#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "processor.h"
#include "memory.h"


/*
 * ------------------------------------------------------------
 * Physical memory
 * ------------------------------------------------------------
 *
 * 8192 bytes total.
 *
 * 16 frames x 512 bytes.
 *
 * Frame 0 is reserved.
 * ------------------------------------------------------------
 */

uint8_t memory[MEMSIZE];


/*
 * ------------------------------------------------------------
 * Frame allocation table
 * ------------------------------------------------------------
 *
 * frame_used[i]:
 *
 *     0 = free
 *     1 = allocated/reserved
 *
 * Frame 0 is permanently reserved.
 * ------------------------------------------------------------
 */

static int frame_used[NUM_FRAMES];


/*
 * ------------------------------------------------------------
 * Per-process page tables
 * ------------------------------------------------------------
 *
 * page_table[p][page] = physical frame
 *
 * -1 means "not mapped".
 * ------------------------------------------------------------
 */

int page_table[NP][NUM_PAGES];


/*
 * ------------------------------------------------------------
 * Utility
 * ------------------------------------------------------------
 */

static int valid_process(int proc_id)
{
    return proc_id >= 0 && proc_id < NP;
}


static int valid_page(int page)
{
    return page >= 0 && page < NUM_PAGES;
}


static int valid_frame(int frame)
{
    return frame >= 0 && frame < NUM_FRAMES;
}


/*
 * ------------------------------------------------------------
 * Initialize page tables
 * ------------------------------------------------------------
 */

void initialize_page_tables(void)
{
    for (int p = 0; p < NP; p++) {

        for (int page = 0; page < NUM_PAGES; page++) {
            page_table[p][page] = -1;
        }
    }
}


/*
 * ------------------------------------------------------------
 * Initialize physical memory
 * ------------------------------------------------------------
 */

void initialize_memory(void)
{
    memset(memory, 0, sizeof(memory));

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        frame_used[frame] = 0;
    }

    /*
     * Frame 0 is reserved.
     */
    frame_used[0] = 1;

    initialize_page_tables();
}


/*
 * ------------------------------------------------------------
 * Get a free physical frame
 * ------------------------------------------------------------
 *
 * Frame 0 is skipped because it is reserved.
 * ------------------------------------------------------------
 */

int getFreePage(void)
{
    for (int frame = 1;
         frame < NUM_FRAMES;
         frame++) {

        if (!frame_used[frame]) {

            frame_used[frame] = 1;

            /*
             * Clear the frame before giving it to a process.
             */
            memset(&memory[frame * PAGE_SIZE],
                   0,
                   PAGE_SIZE);

            return frame;
        }
    }

    /*
     * No frame is available.
     */
    return -1;
}


/*
 * ------------------------------------------------------------
 * Free a physical frame
 * ------------------------------------------------------------
 */

void freePage(int frame)
{
    /*
     * Frame 0 must never be freed.
     */
    if (frame <= 0 || frame >= NUM_FRAMES)
        return;

    if (!frame_used[frame])
        return;

    frame_used[frame] = 0;

    memset(&memory[frame * PAGE_SIZE],
           0,
           PAGE_SIZE);
}


/*
 * ------------------------------------------------------------
 * Map logical page -> physical frame
 * ------------------------------------------------------------
 */

int map_page(int proc_id,
             int logical_page,
             int physical_frame)
{
    if (!valid_process(proc_id))
        return -1;

    if (!valid_page(logical_page))
        return -1;

    if (!valid_frame(physical_frame))
        return -1;

    /*
     * Frame 0 is reserved.
     */
    if (physical_frame == 0)
        return -1;

    /*
     * Do not overwrite an existing mapping.
     */
    if (page_table[proc_id][logical_page] != -1)
        return -1;

    /*
     * The frame must already be allocated.
     */
    if (!frame_used[physical_frame])
        return -1;

    page_table[proc_id][logical_page] =
        physical_frame;

    return 0;
}


/*
 * ------------------------------------------------------------
 * Unmap/free all pages belonging to a process
 * ------------------------------------------------------------
 */

void free_process_pages(int proc_id)
{
    if (!valid_process(proc_id))
        return;

    for (int page = 0;
         page < NUM_PAGES;
         page++) {

        int frame =
            page_table[proc_id][page];

        if (frame != -1) {

            page_table[proc_id][page] = -1;

            freePage(frame);
        }
    }
}


/*
 * ------------------------------------------------------------
 * Logical -> physical address translation
 * ------------------------------------------------------------
 *
 * logical address:
 *
 *     [ page number | page offset ]
 *
 * Since PAGE_SIZE = 512:
 *
 *     page_number = logical_address / 512
 *     offset      = logical_address % 512
 *
 * The resulting physical address is:
 *
 *     physical_frame * 512 + offset
 *
 * isFetch:
 *
 *     1 = instruction access
 *     0 = data access
 *
 * The current page-table representation uses the same logical
 * address space for translation. The parameter is retained so
 * the processor can distinguish instruction and data accesses
 * at the interface level.
 * ------------------------------------------------------------
 */

int getPhysicallAddress(int proc_id,
                        int isFetch,
                        uint32_t logical_address)
{
    (void)isFetch;

    if (!valid_process(proc_id))
        return -1;

    /*
     * Logical addresses outside the process address space
     * are invalid.
     */
    if (logical_address >= LOGICAL_MEMORY_SIZE)
        return -1;

    int logical_page =
        (int)(logical_address / PAGE_SIZE);

    int offset =
        (int)(logical_address % PAGE_SIZE);

    if (!valid_page(logical_page))
        return -1;

    int physical_frame =
        page_table[proc_id][logical_page];

    if (!valid_frame(physical_frame) ||
        physical_frame == 0) {

        return -1;
    }

    return physical_frame * PAGE_SIZE + offset;
}


/*
 * ------------------------------------------------------------
 * Physical byte read
 * ------------------------------------------------------------
 */

int read_physical_byte(int physical_address,
                       uint8_t *value)
{
    if (value == NULL)
        return -1;

    if (physical_address < 0 ||
        physical_address >= MEMSIZE) {

        return -1;
    }

    *value = memory[physical_address];

    return 0;
}


/*
 * ------------------------------------------------------------
 * Physical byte write
 * ------------------------------------------------------------
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
 * ------------------------------------------------------------
 * File loading helper
 * ------------------------------------------------------------
 *
 * Loads a byte-oriented file into a sequence of logical pages.
 *
 * Returns:
 *
 *     0  success
 *    -1  failure
 *
 * max_logical_address specifies the maximum logical address
 * that the file is allowed to occupy.
 * ------------------------------------------------------------
 */

static int load_file_into_process(int proc_id,
                                  const char *filename,
                                  uint32_t logical_start,
                                  uint32_t max_logical_address)
{
    if (!valid_process(proc_id))
        return -1;

    if (filename == NULL)
        return -1;

    FILE *fp = fopen(filename, "rb");

    if (fp == NULL)
        return -1;

    uint32_t logical_address =
        logical_start;

    /*
     * Read one byte at a time.
     *
     * The loop terminates on EOF or when the logical address
     * reaches the explicitly supplied bound.
     */
    while (logical_address < max_logical_address) {

        int c = fgetc(fp);

        if (c == EOF)
            break;

        int physical_address =
            getPhysicallAddress(proc_id,
                                0,
                                logical_address);

        if (physical_address < 0) {

            fclose(fp);
            return -1;
        }

        memory[physical_address] =
            (uint8_t)c;

        logical_address++;
    }

    /*
     * Determine whether the file contained more data than
     * the permitted logical address range.
     *
     * If we stopped because the bound was reached, check
     * whether another byte exists.
     */
    if (logical_address >= max_logical_address) {

        int c = fgetc(fp);

        if (c != EOF) {

            fclose(fp);
            return -1;
        }
    }

    fclose(fp);

    return 0;
}


/*
 * ------------------------------------------------------------
 * Determine file size
 * ------------------------------------------------------------
 */

static long get_file_size(const char *filename)
{
    if (filename == NULL)
        return -1;

    FILE *fp = fopen(filename, "rb");

    if (fp == NULL)
        return -1;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }

    long size = ftell(fp);

    fclose(fp);

    return size;
}


/*
 * ------------------------------------------------------------
 * Allocate pages for a process
 * ------------------------------------------------------------
 *
 * The process needs enough logical pages to hold:
 *
 *     program.byte
 *     data.byte
 *
 * The current logical address space is 4096 bytes, giving
 * eight pages of 512 bytes each.
 * ------------------------------------------------------------
 */

static int allocate_process_pages(int proc_id,
                                  int required_pages)
{
    if (!valid_process(proc_id))
        return -1;

    if (required_pages < 0 ||
        required_pages > NUM_PAGES) {

        return -1;
    }

    for (int page = 0;
         page < required_pages;
         page++) {

        int frame = getFreePage();

        if (frame < 0) {

            /*
             * Roll back any pages already allocated.
             */
            free_process_pages(proc_id);

            return -1;
        }

        if (map_page(proc_id, page, frame) != 0) {

            freePage(frame);
            free_process_pages(proc_id);

            return -1;
        }
    }

    return 0;
}


/*
 * ------------------------------------------------------------
 * Load a complete process
 * ------------------------------------------------------------
 *
 * Program starts at logical address 0.
 *
 * Data follows the program in logical memory.
 *
 * This function allocates only the number of pages actually
 * required, subject to the process logical address-space limit.
 * ------------------------------------------------------------
 */

int load_process_memory(int proc_id,
                        const char *program_file,
                        const char *data_file)
{
    if (!valid_process(proc_id))
        return -1;

    if (program_file == NULL)
        return -1;

    /*
     * Start with a clean address space.
     */
    free_process_pages(proc_id);

    long program_size =
        get_file_size(program_file);

    if (program_size < 0)
        return -1;

    long data_size = 0;

    if (data_file != NULL) {

        data_size =
            get_file_size(data_file);

        if (data_size < 0)
            return -1;
    }

    /*
     * The compiler produces bytecode containing four-byte
     * instructions. Keep the program within the logical
     * address space.
     */
    if (program_size > LOGICAL_MEMORY_SIZE)
        return -1;

    if (data_size > LOGICAL_MEMORY_SIZE)
        return -1;

    /*
     * Data is placed after the program.
     */
    long total_size =
        program_size + data_size;

    if (total_size > LOGICAL_MEMORY_SIZE)
        return -1;

    /*
     * At least one page is required even for an empty program.
     */
    int required_pages =
        (total_size + PAGE_SIZE - 1) / PAGE_SIZE;

    if (required_pages < 1)
        required_pages = 1;

    if (required_pages > NUM_PAGES)
        return -1;

    if (allocate_process_pages(proc_id,
                               required_pages) != 0) {
        return -1;
    }

    /*
     * Load program starting at logical address 0.
     */
    if (load_file_into_process(proc_id,
                               program_file,
                               0,
                               (uint32_t)program_size) != 0) {

        free_process_pages(proc_id);
        return -1;
    }

    /*
     * Load data immediately after program.
     */
    if (data_file != NULL && data_size > 0) {

        if (load_file_into_process(proc_id,
                                   data_file,
                                   (uint32_t)program_size,
                                   (uint32_t)total_size) != 0) {

            free_process_pages(proc_id);
            return -1;
        }
    }

    return 0;
}


/*
 * ------------------------------------------------------------
 * Unload process
 * ------------------------------------------------------------
 */

void unload_process_memory(int proc_id)
{
    if (!valid_process(proc_id))
        return;

    free_process_pages(proc_id);
}


/*
 * ------------------------------------------------------------
 * Finalize memory
 * ------------------------------------------------------------
 */

void finalize_memory(void)
{
    /*
     * Release all process mappings.
     */
    for (int p = 0; p < NP; p++) {
        free_process_pages(p);
    }

    /*
     * Frame 0 remains reserved.
     */
    memset(memory, 0, sizeof(memory));
}
