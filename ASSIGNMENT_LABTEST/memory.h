#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include "processor.h"

#define MEMSIZE             8192
#define PAGESIZE            512
#define INSTRUCTION_SIZE    1024
#define DATA_SIZE           4096
#define NUM_LOGICAL_PAGES   10
#define NUM_PHYSICAL_PAGES  (MEMSIZE / PAGESIZE)

extern uint8_t memory[MEMSIZE];
extern int pageTable[NP][NUM_LOGICAL_PAGES];
extern int freePages[NUM_PHYSICAL_PAGES];

void initialize_memory(void);
void finalize_memory(void);

int load_process_memory(int proc_id,
                        const char *program_file,
                        const char *data_file);

void unload_process_memory(int proc_id);
void release_process_memory(int proc_id);

int getFreePage(void);
void freePage(int frame);

int getPhysicallAddress(int proc_id,
                        int isFetch,
                        int address);

int read_physical_byte(int physical_address,
                       uint8_t *value);

int write_physical_byte(int physical_address,
                        uint8_t value);

/* Logical process-memory accessors. These transparently perform paging. */
int read_process_byte(int proc_id,
                      int isFetch,
                      int logical_address,
                      uint8_t *value);

int write_process_byte(int proc_id,
                       int logical_address,
                       uint8_t value);

/* Explicit page operations, useful for diagnostics/tests. */
int page_in(int proc_id, int logical_page);
int page_out(int proc_id, int logical_page);

void initialize(void);
void finalize(void);

#endif
