#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <stdint.h>


/*
 * ============================================================
 * Processor configuration
 * ============================================================
 *
 * NP is the number of simulated processors and is expected to
 * be supplied by the build/starter configuration.
 *
 * If it has not already been defined elsewhere, use the
 * assignment's default processor count.
 * ============================================================
 */

#ifndef NP
#define NP 4
#endif


/*
 * ============================================================
 * Processor registers
 * ============================================================
 *
 * One complete register set exists for every simulated
 * processor.
 * ============================================================
 */

/*
 * Program Counter
 *
 * Contains the logical instruction address of the next
 * instruction.
 */
extern int PC[NP];


/*
 * Stack Pointer
 *
 * Addresses logical data memory.
 */
extern int SP[NP];


/*
 * Accumulator
 *
 * General-purpose arithmetic register.
 */
extern int AC[NP];


/*
 * Instruction Register
 *
 * Holds the currently fetched four-byte instruction.
 */
extern int IR[NP];


/*
 * Processor status.
 *
 * 0 = running
 * 1 = halted
 * 2 = processor error
 */
extern int status[NP];


/*
 * Base address/register retained for compatibility with the
 * starter processor interface.
 */
extern int baseAddress[NP];


/*
 * Indicates that the current process has terminated and that
 * the processor can be released by the OS.
 *
 * 0 = still executing
 * 1 = process has stopped
 */
extern int end_of_simulation[NP];


/*
 * ============================================================
 * Processor lifecycle
 * ============================================================
 */

/*
 * Initialize all processor registers.
 */
void initialize_processor(void);


/*
 * Reset one processor before a process is dispatched to it.
 */
void reset(int proc_id);


/*
 * ============================================================
 * Instruction execution
 * ============================================================
 *
 * Execute at most instruction_count instructions.
 *
 * The function MUST return after instruction_count
 * instructions even if the user program contains an infinite
 * branch loop.
 */
void process_instructions(int proc_id,
                          int instruction_count);


/*
 * ============================================================
 * Processor termination
 * ============================================================
 *
 * The processor implementation sets end_of_simulation[proc_id]
 * when the currently running process executes its terminating
 * instruction or encounters an unrecoverable execution error.
 *
 * The OS uses that flag to determine when the processor can be
 * released.
 * ============================================================
 */

#endif
