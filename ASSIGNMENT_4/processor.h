#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <stdint.h>

#define NP 4
#define INT_REGS 256
#define VEC_REGS 32
#define VEC_LANES 8

extern uint32_t RegisterFile[NP][INT_REGS];
extern uint32_t VectorRegisterFile[NP][VEC_REGS][VEC_LANES];
extern uint32_t PC[NP];
extern uint8_t opcode[NP], dest[NP], src1[NP], src2[NP];
extern int N[NP], Z[NP], C[NP], V[NP];
extern int end_of_simulation[NP];

void reset(int proc_id);
void fetch(int proc_id);
void decode(int proc_id);
void execute(int proc_id);
void process_instructions(int proc_id, int instruction_count);

#endif
