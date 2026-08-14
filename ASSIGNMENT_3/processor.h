#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <stdint.h>

/* Integer register file */
extern uint32_t RegisterFile[256];

/* Vector register file: 32 registers, each with 8 x 32-bit elements */
extern uint32_t VectorRegisterFile[32][8];

/* Processor state */
extern uint32_t PC;
extern uint8_t opcode, dest, src1, src2;

/* Condition flags */
extern int N, Z, C, V;

/* End of simulation flag */
extern int end_of_simulation;

/* Pipeline stages */
void reset();
void fetch();
void decode();
void execute();

#endif
