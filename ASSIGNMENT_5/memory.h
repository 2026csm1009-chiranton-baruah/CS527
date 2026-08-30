#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

/*
 * ============================================================
 * Memory configuration from Lab 5
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
 * Instruction memory:
 *
 *     1024 bytes
 *
 * Data memory:
 *
 *     4096 bytes
 *
 * Logical pages:
 *
 *     1024 / 512 + 4096 / 512
 *     = 2 + 8
 *     = 10 pages
 *
 * Physical frames:
 *
 *     8192 / 512
 *     = 16 frames
 *
 * Frame 0 is reserved.
 * ============================================================
 */

#define MEMSIZE             8192
#define PAGESIZE            512

#define INSTRUCTION_SIZE    1024
#define DATA_SIZE           4096

#define NUM_LOGICAL_PAGES   10
#define NUM_PHYSICAL_PAGES  (MEMSIZE / PAGESIZE)


/*
 * ============================================================
 * Physical memory
 * ============================================================
 *
 * All instruction and data accesses eventually reach this
 * physical memory.
 * ============================================================
 */

extern uint8_t memory[MEMSIZE];


/*
 * ============================================================
 * Page tables
 * ============================================================
 *
 * pageTable[process][logical_page]
 *
 * stores the physical frame number corresponding to a logical
 * page.
 *
 * -1 means that the logical page is not currently mapped.
 * ============================================================
 */

extern int pageTable[NP][NUM_LOGICAL_PAGES];


/*
 * ============================================================
 * Free-frame table
 * ============================================================
 *
 * freePages[frame]:
 *
 *     0 = free
 *     1 = allocated
 *
 * Frame 0 is permanently reserved.
 * ============================================================
 */

extern int freePages[NUM_PHYSICAL_PAGES];


/*
 * ============================================================
 * Memory initialization
 * ============================================================
 */

void initialize_memory(int proc_id,
                       const char *program_file,
                       const char *data_file);


/*
 * ============================================================
 * Memory finalization
 * ============================================================
 *
 * Writes the process's data memory back to its data file and
 * releases its physical frames.
 * ============================================================
 */

void finalize_memory(int proc_id,
                     const char *data_file);


/*
 * ============================================================
 * Physical-frame allocation
 * ============================================================
 *
 * Returns:
 *
 *     physical frame number >= 1
 *     -1 if no frame is available
 *
 * Frame 0 is never returned.
 * ============================================================
 */

int getFreePage(void);


/*
 * Release a previously allocated physical frame.
 */
void freePage(int frame);


/*
 * ============================================================
 * Logical -> physical address translation
 * ============================================================
 *
 * isFetch:
 *
 *     1 = instruction-memory access
 *     0 = data-memory access
 *
 * address:
 *
 *     logical byte address
 *
 * Returns:
 *
 *     physical byte address
 *     -1 on invalid/unmapped address
 *
 * Lab 5 specifies:
 *
 *     Instruction page =
 *         address / PAGESIZE
 *
 *     Data page =
 *         address / PAGESIZE + INSTRUCTION_SIZE / PAGESIZE
 *
 *     Physical address =
 *         physical_frame * PAGESIZE
 *         + address % PAGESIZE
 * ============================================================
 */

int getPhysicallAddress(int proc_id,
                        int isFetch,
                        int address);


/*
 * ============================================================
 * Physical-memory byte access
 * ============================================================
 */

int read_physical_byte(int physical_address,
                       uint8_t *value);

int write_physical_byte(int physical_address,
                        uint8_t value);


/*
 * ============================================================
 * Process cleanup
 * ============================================================
 */

void release_process_memory(int proc_id);


/*
 * ============================================================
 * Compatibility interface
 * ============================================================
 *
 * These are retained because the starter memory.c already
 * exposes initialize()/finalize().
 *
 * OS-level code should use initialize_memory() and
 * finalize_memory().
 * ============================================================
 */

void initialize(void);
void finalize(void);

#endif
