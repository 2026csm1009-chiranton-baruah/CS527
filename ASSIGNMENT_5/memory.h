#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

/*
 * ------------------------------------------------------------
 * Physical memory
 * ------------------------------------------------------------
 *
 * Total physical memory:
 *
 *     8192 bytes
 *
 * Page/frame size:
 *
 *     512 bytes
 *
 * Therefore:
 *
 *     8192 / 512 = 16 frames
 *
 * Frame 0 is reserved by the operating system.
 * ------------------------------------------------------------
 */

#define MEMSIZE       8192
#define PAGE_SIZE     512
#define NUM_FRAMES    (MEMSIZE / PAGE_SIZE)

/*
 * The logical address space of each process is also divided
 * into 512-byte pages.
 *
 * A process therefore has:
 *
 *     4096 / 512 = 8 logical pages
 *
 * for the 4096-byte instruction/data address space used by
 * the starter implementation.
 */
#define LOGICAL_MEMORY_SIZE 4096
#define NUM_PAGES           (LOGICAL_MEMORY_SIZE / PAGE_SIZE)


/*
 * ------------------------------------------------------------
 * Physical memory array
 * ------------------------------------------------------------
 *
 * memory[0] ... memory[8191]
 *
 * is the actual simulated physical memory.
 * ------------------------------------------------------------
 */

extern uint8_t memory[MEMSIZE];


/*
 * ------------------------------------------------------------
 * Page table
 * ------------------------------------------------------------
 *
 * page_table[process][virtual_page] = physical_frame
 *
 * A value of -1 means that the virtual page is currently
 * unmapped.
 * ------------------------------------------------------------
 */

extern int page_table[NP][NUM_PAGES];


/*
 * ------------------------------------------------------------
 * Memory initialization/finalization
 * ------------------------------------------------------------
 */

void initialize_memory(void);
void finalize_memory(void);


/*
 * ------------------------------------------------------------
 * Physical-frame management
 * ------------------------------------------------------------
 *
 * getFreePage()
 *
 * Returns a free physical frame number.
 *
 * Frame 0 is reserved and must never be returned.
 *
 * Returns:
 *
 *     frame number >= 1   success
 *     -1                  no free frame
 * ------------------------------------------------------------
 */

int getFreePage(void);


/*
 * Release a physical frame.
 */
void freePage(int frame);


/*
 * ------------------------------------------------------------
 * Page-table management
 * ------------------------------------------------------------
 */

/*
 * Initialize all page tables.
 */
void initialize_page_tables(void);


/*
 * Release every physical frame belonging to a process.
 */
void free_process_pages(int proc_id);


/*
 * Map one logical page to a physical frame.
 *
 * Returns:
 *
 *     0   success
 *    -1   failure
 */
int map_page(int proc_id,
             int logical_page,
             int physical_frame);


/*
 * ------------------------------------------------------------
 * Address translation
 * ------------------------------------------------------------
 *
 * Translate:
 *
 *     logical address -> physical address
 *
 * isFetch:
 *
 *     1 = instruction access
 *     0 = data access
 *
 * Returns:
 *
 *     physical byte address >= 0
 *     -1 on invalid/unmapped address
 *
 * The intentionally unusual spelling
 * getPhysicallAddress()
 * is retained because that is the interface expected by the
 * existing processor code/starter architecture.
 * ------------------------------------------------------------
 */

int getPhysicallAddress(int proc_id,
                        int isFetch,
                        uint32_t logical_address);


/*
 * ------------------------------------------------------------
 * Program/data loading
 * ------------------------------------------------------------
 *
 * program_file:
 *     compiled program.byte
 *
 * data_file:
 *     optional data.byte
 *
 * The loader will allocate physical frames and establish the
 * process page-table mappings before copying the contents.
 * ------------------------------------------------------------
 */

int load_process_memory(int proc_id,
                        const char *program_file,
                        const char *data_file);


/*
 * Unload a process from physical memory.
 */
void unload_process_memory(int proc_id);


/*
 * ------------------------------------------------------------
 * Physical memory byte access
 * ------------------------------------------------------------
 *
 * These helpers are useful to processor/OS code that needs
 * controlled physical-memory access.
 * ------------------------------------------------------------
 */

int read_physical_byte(int physical_address,
                       uint8_t *value);

int write_physical_byte(int physical_address,
                        uint8_t value);

#endif
