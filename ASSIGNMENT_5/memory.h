#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include "processor.h"

#define MEMSIZE 8192
#define PAGESIZE 512
#define INSTR_MEMSIZE 1024
#define DATA_MEMSIZE 4096
#define NUM_LOGICAL_PAGES ((INSTR_MEMSIZE + DATA_MEMSIZE) / PAGESIZE)
#define NUM_PHYSICAL_PAGES (MEMSIZE / PAGESIZE)

/* One page table per processor/process.  A value of -1 means unmapped. */
extern int pageTable[NP][NUM_LOGICAL_PAGES];
extern uint8_t memory[NP][MEMSIZE];

int getPhysicallAddress(int proc_id, int isFetch, int address);
int getFreePage(int proc_id);

void initialize_memory(int proc_id, const char *program_file, const char *data_file);
void memory_system_init(void);
void finalize_memory(int proc_id, const char *data_file);

/* Compatibility helpers for the old single-process interface. */
void initialize(void);
void finalize(void);

#endif
