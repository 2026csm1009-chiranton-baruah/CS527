#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <stdint.h>

extern uint32_t RegisterFile[256];
extern uint32_t PC;
extern uint8_t opcode, dest, src1, src2;
extern int N, Z, C, V;
extern int end_of_simulation;

void reset();
void fetch();
void decode();
void execute();

#endif
