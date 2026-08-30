#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "processor.h"


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
 * PAGE TABLES
 * ============================================================
 *
 * Each process has 10 logical pages:
 *
 *     Page 0 - 1 : instruction memory
 *     Page 2 - 9 : data memory
 *
 * Value:
 *
 *     >= 1 : physical frame number
 *     -1   : page not mapped
 *
 * Physical frame 0 is reserved.
 * ============================================================
 */

int pageTable[NP][NUM_LOGICAL_PAGES];


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

    /*
     * NUM_LOGICAL_PAGES is fixed at 10.
     * Therefore this loop is always bounded.
     */
    for (int page = 0;
         page < NUM_LOGICAL_PAGES;
         page++) {

        pageTable[proc_id][page] = -1;
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


    /*
     * Clear every process page table.
     */
    for (int proc = 0;
         proc < NP;
         proc++) {

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
     * Look up physical frame.
     */
    int physical_frame =
        pageTable[proc_id][logical_page];


    /*
     * -1 means page is unmapped.
     */
    if (!valid_frame(physical_frame))
        return -1;


    /*
     * Frame 0 is reserved.
     */
    if (physical_frame == 0)
        return -1;


    /*
     * Frame must currently be allocated.
     */
    if (freePages[physical_frame] == 0)
        return -1;


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
            getFreePage();


        if (frame < 0) {

            /*
             * Roll back pages allocated so far.
             */
            release_process_memory(proc_id);

            return -1;
        }


        pageTable[proc_id][page] =
            frame;
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
            getFreePage();


        if (frame < 0) {

            /*
             * Roll back all pages allocated for this process.
             */
            release_process_memory(proc_id);

            return -1;
        }


        pageTable[proc_id]
                 [first_data_page + page] =
            frame;
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
     * No data pages are required if data.byte is empty.
     */
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
 * SAVE PROCESS DATA MEMORY
 * ============================================================
 *
 * Writes the logical data address space to the specified
 * data file.
 *
 * Four bytes are written per line.
 *
 * DATA_SIZE = 4096
 *
 * Therefore:
 *
 *     4096 / 4 = 1024 lines maximum.
 * ============================================================
 */

static int save_data_file(int proc_id,
                          const char *data_file)
{
    if (!valid_proc(proc_id))
        return -1;


    if (data_file == NULL)
        return -1;


    FILE *fp =
        fopen(data_file, "w");


    if (fp == NULL)
        return -1;


    /*
     * Four bytes per line.
     *
     * Maximum iterations = 1024.
     */
    for (int address = 0;
         address < DATA_SIZE;
         address += 4) {

        uint8_t bytes[4] =
            {0, 0, 0, 0};


        /*
         * Four bytes within the current line.
         */
        for (int i = 0;
             i < 4;
             i++) {

            int logical_address =
                address + i;


            if (logical_address >= DATA_SIZE)
                break;


            int physical =
                getPhysicallAddress(proc_id,
                                    0,
                                    logical_address);


            /*
             * Unmapped pages are represented by zeroes.
             */
            if (physical >= 0) {

                bytes[i] =
                    memory[physical];
            }
        }


        fprintf(fp,
                "%02X %02X %02X %02X\n",
                bytes[0],
                bytes[1],
                bytes[2],
                bytes[3]);
    }


    fclose(fp);

    return 0;
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
    for (int page = 0;
         page < NUM_LOGICAL_PAGES;
         page++) {

        int frame =
            pageTable[proc_id][page];


        /*
         * Release valid process-owned frames.
         */
        if (frame > 0 &&
            frame < NUM_PHYSICAL_PAGES) {

            freePage(frame);
        }


        /*
         * Remove mapping.
         */
        pageTable[proc_id][page] =
            -1;
    }
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
