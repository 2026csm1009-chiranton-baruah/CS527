#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "processor.h"


/*
 * ============================================================
 * Physical memory
 * ============================================================
 *
 * 8192 bytes / 512 bytes per frame = 16 frames.
 *
 * Frame 0 is reserved.
 * ============================================================
 */

uint8_t memory[MEMSIZE];


/*
 * ============================================================
 * Page tables
 * ============================================================
 *
 * Each processor/process has:
 *
 *     2 instruction pages
 *   + 8 data pages
 *   = 10 logical pages
 *
 * pageTable[proc_id][logical_page] contains the physical
 * frame number.
 *
 * -1 means the logical page is not mapped.
 * ============================================================
 */

int pageTable[NP][NUM_LOGICAL_PAGES];


/*
 * ============================================================
 * Physical-frame allocation table
 * ============================================================
 *
 * freePages[frame]:
 *
 *     0 = free
 *     1 = allocated
 *
 * Frame 0 is permanently marked allocated/reserved.
 * ============================================================
 */

int freePages[NUM_PHYSICAL_PAGES];


/*
 * ============================================================
 * Validation helpers
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
 * Clear one process's page table
 * ============================================================
 */

static void clear_page_table(int proc_id)
{
    if (!valid_proc(proc_id))
        return;

    for (int page = 0;
         page < NUM_LOGICAL_PAGES;
         page++) {

        pageTable[proc_id][page] = -1;
    }
}


/*
 * ============================================================
 * Initialize global physical memory state
 * ============================================================
 */

static void initialize_physical_memory(void)
{
    memset(memory,
           0,
           sizeof(memory));

    for (int frame = 0;
         frame < NUM_PHYSICAL_PAGES;
         frame++) {

        freePages[frame] = 0;
    }

    /*
     * Frame 0 is reserved forever.
     */
    freePages[0] = 1;

    for (int proc = 0;
         proc < NP;
         proc++) {

        clear_page_table(proc);
    }
}


/*
 * ============================================================
 * Read byte-oriented hexadecimal file
 * ============================================================
 *
 * The bytecode/data files contain hexadecimal bytes.
 *
 * Example:
 *
 *     FF FF FF FF
 *
 * is four bytes.
 *
 * max_bytes prevents the loader from exceeding the logical
 * memory region.
 * ============================================================
 */

static int load_hex_file(const char *filename,
                         uint8_t *destination,
                         int max_bytes,
                         int *bytes_loaded)
{
    if (filename == NULL ||
        destination == NULL ||
        bytes_loaded == NULL)
        return -1;

    if (max_bytes < 0)
        return -1;

    FILE *fp = fopen(filename, "r");

    if (fp == NULL)
        return -1;

    int count = 0;
    unsigned int value;

    /*
     * Every iteration consumes one hexadecimal byte.
     *
     * The count is explicitly bounded by max_bytes.
     */
    while (count < max_bytes) {

        int result =
            fscanf(fp, "%x", &value);

        if (result != 1)
            break;

        /*
         * Byte files must contain values in the range 0..255.
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
     * If fscanf stopped because of malformed non-whitespace
     * input, reject the file.
     */
    if (!feof(fp)) {

        /*
         * Try to consume whitespace. If another character
         * remains, the file is malformed.
         */
        int c;

        do {
            c = fgetc(fp);
        } while (c == ' ' ||
                 c == '\t' ||
                 c == '\n' ||
                 c == '\r');

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
 * Get number of pages required
 * ============================================================
 *
 * ceil(bytes / PAGESIZE)
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
 * Allocate a physical frame
 * ============================================================
 *
 * Lab requirement:
 *
 *     iterate through free-frame list
 *     return first available frame
 *
 * Frame 0 is skipped because it is reserved.
 * ============================================================
 */

int getFreePage(void)
{
    for (int frame = 1;
         frame < NUM_PHYSICAL_PAGES;
         frame++) {

        if (freePages[frame] == 0) {

            freePages[frame] = 1;

            /*
             * Clear the frame before assigning it.
             */
            memset(&memory[frame * PAGESIZE],
                   0,
                   PAGESIZE);

            return frame;
        }
    }

    /*
     * No physical frame remains.
     *
     * Do not loop or retry indefinitely.
     */
    fprintf(stderr,
            "ERROR: no free physical page/frame available\n");

    return -1;
}


/*
 * ============================================================
 * Free physical frame
 * ============================================================
 */

void freePage(int frame)
{
    /*
     * Frame 0 is reserved and cannot be released.
     */
    if (frame <= 0 ||
        frame >= NUM_PHYSICAL_PAGES)
        return;

    if (freePages[frame] == 0)
        return;

    freePages[frame] = 0;

    memset(&memory[frame * PAGESIZE],
           0,
           PAGESIZE);
}


/*
 * ============================================================
 * Translate logical address -> physical address
 * ============================================================
 *
 * This follows the exact structure specified by Lab 5:
 *
 *     Index = (isFetch)
 *           ? address / PAGESIZE
 *           : address / PAGESIZE + 1024 / PAGESIZE
 *
 *     Physical address =
 *         physical page number * PAGESIZE
 *         + address % PAGESIZE
 *
 * Instruction memory:
 *
 *     logical 0 .. 1023
 *
 * Data memory:
 *
 *     logical 0 .. 4095
 *
 * isFetch determines which logical region the address belongs
 * to.
 * ============================================================
 */

int getPhysicallAddress(int proc_id,
                        int isFetch,
                        int address)
{
    if (!valid_proc(proc_id))
        return -1;

    if (address < 0)
        return -1;


    int logical_page;
    int offset;


    /*
     * --------------------------------------------------------
     * Instruction access
     * --------------------------------------------------------
     *
     * Instruction logical addresses are 0..1023.
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
     * Data access
     * --------------------------------------------------------
     *
     * Data logical addresses are independently based at zero.
     *
     * Page-table indices are shifted by two pages because
     * instruction memory occupies:
     *
     *     1024 / 512 = 2 pages
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


    if (!valid_logical_page(logical_page))
        return -1;


    /*
     * Retrieve physical frame from page table.
     */
    int physical_frame =
        pageTable[proc_id][logical_page];

    if (!valid_frame(physical_frame))
        return -1;

    /*
     * Frame 0 must never be used for a process.
     */
    if (physical_frame == 0)
        return -1;

    /*
     * A frame that isn't allocated cannot be accessed.
     */
    if (freePages[physical_frame] == 0)
        return -1;


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
 * Allocate and map instruction pages
 * ============================================================
 */

static int allocate_instruction_pages(int proc_id,
                                      int bytes)
{
    if (!valid_proc(proc_id))
        return -1;

    int required =
        pages_required(bytes);

    if (required > INSTRUCTION_SIZE / PAGESIZE)
        return -1;

    for (int page = 0;
         page < required;
         page++) {

        int frame =
            getFreePage();

        if (frame < 0) {

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
 * Allocate and map data pages
 * ============================================================
 */

static int allocate_data_pages(int proc_id,
                               int bytes)
{
    if (!valid_proc(proc_id))
        return -1;

    int required =
        pages_required(bytes);

    if (required > DATA_SIZE / PAGESIZE)
        return -1;

    int first_data_page =
        INSTRUCTION_SIZE / PAGESIZE;

    for (int page = 0;
         page < required;
         page++) {

        int frame =
            getFreePage();

        if (frame < 0) {

            release_process_memory(proc_id);

            return -1;
        }

        pageTable[proc_id][first_data_page + page] =
            frame;
    }

    return 0;
}


/*
 * ============================================================
 * Copy bytes into physical instruction pages
 * ============================================================
 */

static int copy_instruction_bytes(int proc_id,
                                  const uint8_t *bytes,
                                  int count)
{
    if (!valid_proc(proc_id) ||
        bytes == NULL ||
        count < 0 ||
        count > INSTRUCTION_SIZE)
        return -1;

    for (int i = 0;
         i < count;
         i++) {

        int physical =
            getPhysicallAddress(proc_id,
                                1,
                                i);

        if (physical < 0)
            return -1;

        memory[physical] =
            bytes[i];
    }

    return 0;
}


/*
 * ============================================================
 * Copy bytes into physical data pages
 * ============================================================
 */

static int copy_data_bytes(int proc_id,
                           const uint8_t *bytes,
                           int count)
{
    if (!valid_proc(proc_id) ||
        bytes == NULL ||
        count < 0 ||
        count > DATA_SIZE)
        return -1;

    for (int i = 0;
         i < count;
         i++) {

        int physical =
            getPhysicallAddress(proc_id,
                                0,
                                i);

        if (physical < 0)
            return -1;

        memory[physical] =
            bytes[i];
    }

    return 0;
}


/*
 * ============================================================
 * Initialize process memory
 * ============================================================
 *
 * The lab specifies that initialization:
 *
 * 1. reads program.byte
 * 2. determines required instruction pages
 * 3. allocates physical frames
 * 4. updates the page table
 * 5. reads data.byte
 * 6. determines required data pages
 * 7. allocates physical frames
 * 8. updates the page table
 *
 * We perform the same process for the supplied filenames.
 * ============================================================
 */

void initialize_memory(int proc_id,
                       const char *program_file,
                       const char *data_file)
{
    if (!valid_proc(proc_id))
        return;

    if (program_file == NULL)
        return;


    /*
     * Start with a clean process address space.
     */
    release_process_memory(proc_id);


    /*
     * Temporary buffers contain the logical contents before
     * those bytes are copied into their physical frames.
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
     * Program
     * --------------------------------------------------------
     */

    if (load_hex_file(program_file,
                      instruction_bytes,
                      INSTRUCTION_SIZE,
                      &instruction_count) != 0) {

        fprintf(stderr,
                "ERROR: cannot load program file %s\n",
                program_file);

        return;
    }


    /*
     * The program must fit within the two instruction pages.
     */
    if (instruction_count > INSTRUCTION_SIZE) {

        fprintf(stderr,
                "ERROR: program exceeds instruction memory\n");

        return;
    }


    /*
     * At least one instruction page is useful for an empty
     * or minimal program image.
     */
    int instruction_pages =
        pages_required(instruction_count);

    if (instruction_pages == 0)
        instruction_pages = 1;


    if (allocate_instruction_pages(proc_id,
                                   instruction_count) != 0) {

        fprintf(stderr,
                "ERROR: unable to allocate instruction pages\n");

        release_process_memory(proc_id);

        return;
    }


    /*
     * Copy program bytes into their physical frames.
     */
    if (copy_instruction_bytes(proc_id,
                               instruction_bytes,
                               instruction_count) != 0) {

        fprintf(stderr,
                "ERROR: failed to copy program into memory\n");

        release_process_memory(proc_id);

        return;
    }


    /*
     * --------------------------------------------------------
     * Data
     * --------------------------------------------------------
     *
     * data.byte is optional.
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

            return;
        }
    }


    /*
     * Allocate enough physical frames for data.
     *
     * An empty data file needs no data frame.
     */
    if (data_count > 0) {

        if (allocate_data_pages(proc_id,
                                data_count) != 0) {

            fprintf(stderr,
                    "ERROR: unable to allocate data pages\n");

            release_process_memory(proc_id);

            return;
        }


        if (copy_data_bytes(proc_id,
                            data_bytes,
                            data_count) != 0) {

            fprintf(stderr,
                    "ERROR: failed to copy data into memory\n");

            release_process_memory(proc_id);

            return;
        }
    }
}


/*
 * ============================================================
 * Write logical data memory back to data.byte
 * ============================================================
 *
 * Data memory is always addressed logically from 0.
 *
 * Only the mapped data pages are accessed.
 *
 * The output format remains four hexadecimal bytes per line,
 * matching the bytecode/data-file format in the lab.
 * ============================================================
 */

static int save_data_file(int proc_id,
                          const char *data_file)
{
    if (!valid_proc(proc_id) ||
        data_file == NULL)
        return -1;

    FILE *fp =
        fopen(data_file, "w");

    if (fp == NULL)
        return -1;


    /*
     * Data memory is 4096 bytes.
     *
     * Iterate in groups of four because the lab's data-file
     * format stores four bytes per line.
     */
    for (int address = 0;
         address < DATA_SIZE;
         address += 4) {

        uint8_t bytes[4] = {0, 0, 0, 0};

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

            if (physical < 0) {

                /*
                 * Unmapped portions of the logical data space
                 * are represented as zeroes.
                 */
                bytes[i] = 0;

            } else {

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
 * Release all physical pages belonging to a process
 * ============================================================
 */

void release_process_memory(int proc_id)
{
    if (!valid_proc(proc_id))
        return;

    for (int page = 0;
         page < NUM_LOGICAL_PAGES;
         page++) {

        int frame =
            pageTable[proc_id][page];

        if (frame > 0 &&
            frame < NUM_PHYSICAL_PAGES) {

            freePage(frame);
        }

        pageTable[proc_id][page] = -1;
    }
}


/*
 * ============================================================
 * Finalize process memory
 * ============================================================
 *
 * The lab specifies that finalization writes data.byte and
 * releases the pages used by the task.
 * ============================================================
 */

void finalize_memory(int proc_id,
                     const char *data_file)
{
    if (!valid_proc(proc_id))
        return;

    /*
     * Save the process's modified logical data memory before
     * releasing its physical frames.
     */
    if (data_file != NULL &&
        data_file[0] != '\0') {

        if (save_data_file(proc_id,
                           data_file) != 0) {

            fprintf(stderr,
                    "WARNING: could not save data file %s\n",
                    data_file);
        }
    }

    release_process_memory(proc_id);
}


/*
 * ============================================================
 * Physical byte read
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
 * Physical byte write
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
 * Legacy compatibility initialize()
 * ============================================================
 *
 * The lab says initialize/finalize move into the OS layer.
 *
 * This wrapper is retained so older starter-code references
 * do not cause a linker error.
 * ============================================================
 */

void initialize(void)
{
    initialize_physical_memory();

    /*
     * Processor 0 receives the traditional files.
     */
    initialize_memory(0,
                      "program.byte",
                      "data.byte");
}


/*
 * ============================================================
 * Legacy compatibility finalize()
 * ============================================================
 */

void finalize(void)
{
    finalize_memory(0,
                    "data.byte");
}
