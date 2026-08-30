#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include "processor.h"


/*
 * ============================================================
 * Physical memory configuration
 * ============================================================
 */

#define MEMSIZE             8192
#define PAGESIZE            512

#define INSTRUCTION_SIZE    1024
#define DATA_SIZE           4096

#define NUM_LOGICAL_PAGES  10
#define NUM_PHYSICAL_PAGES (MEMSIZE / PAGESIZE)


/*
 * ============================================================
 * Physical memory
 * ============================================================
 *
 * Physical memory consists of 16 frames of 512 bytes each.
 *
 * Frame 0 is reserved by the operating system.
 * ============================================================
 */

extern uint8_t memory[MEMSIZE];


/*
 * ============================================================
 * Page table
 * ============================================================
 *
 * There are 10 logical pages per process:
 *
 *     Pages 0-1 : instruction memory
 *     Pages 2-9 : data memory
 *
 * A value of -1 means that the logical page is not mapped.
 * ============================================================
 */

extern int pageTable[NP][NUM_LOGICAL_PAGES];


/*
 * ============================================================
 * Physical-frame allocation table
 * ============================================================
 *
 *     0 = free
 *     1 = allocated/reserved
 *
 * Frame 0 is permanently reserved.
 * ============================================================
 */

extern int freePages[NUM_PHYSICAL_PAGES];


/*
 * ============================================================
 * Global memory initialization/finalization
 * ============================================================
 *
 * These are called by the OS.
 * ============================================================
 */

void initialize_memory(void);

void finalize_memory(void);


/*
 * ============================================================
 * Process memory management
 * ============================================================
 *
 * load_process_memory()
 *
 *     Loads the program and data images belonging to a process
 *     into physical memory and establishes its page table.
 *
 * Returns:
 *
 *     0  = success
 *    -1  = failure
 *
 *
 * unload_process_memory()
 *
 *     Releases all physical pages belonging to the process.
 * ============================================================
 */

int load_process_memory(int proc_id,
                        const char *program_file,
                        const char *data_file);

void unload_process_memory(int proc_id);


/*
 * ============================================================
 * Physical-frame allocation
 * ============================================================
 */

int getFreePage(void);

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
 * ============================================================
 */

int getPhysicallAddress(int proc_id,
                        int isFetch,
                        int address);


/*
 * ============================================================
 * Physical byte access
 * ============================================================
 */

int read_physical_byte(int physical_address,
                       uint8_t *value);

int write_physical_byte(int physical_address,
                        uint8_t value);


/*
 * ============================================================
 * Process memory cleanup
 * ============================================================
 */

void release_process_memory(int proc_id);


/*
 * ============================================================
 * Compatibility functions
 * ============================================================
 *
 * Kept for compatibility with older starter-code references.
 * ============================================================
 */

void initialize(void);

void finalize(void);

#endif
