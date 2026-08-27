#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include "processor.h"

extern uint8_t Instruction[NP][256];
extern uint8_t Data[NP][4096];

void initialize_memory(int proc_id, const char *program_file, const char *data_file);
void finalize_memory(int proc_id, const char *data_file);
void initialize(void);
void finalize(void);

#endif
