#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <stdint.h>

/*
 * Number of processors in the simulated system.
 * This is fixed at compile time as required by the lab.
 */
#define NP 4

/*
 * Integer register file:
 * x0 - x255
 */
#define INT_REGS 256

/*
 * Vector register file:
 * v0 - v31
 *
 * Each vector register is 256 bits = 8 x 32-bit integers.
 */
#define VEC_REGS 32
#define VEC_LANES 8

/*
 * Processor instruction-memory size.
 *
 * Each instruction occupies four bytes.
 * 256 bytes therefore gives space for 64 instructions.
 */
#define INSTRUCTION_MEMORY_SIZE 256

/*
 * Processor functions.
 */
void reset(int proc_id);
void fetch(int proc_id);
void decode(int proc_id);
void execute(int proc_id);

/*
 * Execute at most instruction_count instructions.
 *
 * The scheduler uses this to implement the time slice.
 */
void process_instructions(int proc_id, int instruction_count);

/*
 * Integer register file.
 */
extern uint32_t RegisterFile[NP][INT_REGS];

/*
 * Vector register file.
 *
 * VectorRegisterFile[processor][vector register][lane]
 */
extern uint32_t VectorRegisterFile[NP][VEC_REGS][VEC_LANES];

/*
 * Program counters.
 *
 * One PC for each simulated processor.
 */
extern uint32_t PC[NP];

/*
 * Current fetched instruction.
 */
extern uint8_t opcode[NP];
extern uint8_t dest[NP];
extern uint8_t src1[NP];
extern uint8_t src2[NP];

/*
 * Arithmetic condition flags.
 *
 * Z = Zero
 * N = Negative
 * C = Carry
 * V = Overflow
 */
extern int N[NP];
extern int Z[NP];
extern int C[NP];
extern int V[NP];

/*
 * Set to 1 when a processor reaches the termination
 * instruction or is otherwise stopped safely.
 */
extern int end_of_simulation[NP];

#endif
