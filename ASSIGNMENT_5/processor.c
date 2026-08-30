#define _DEFAULT_SOURCE

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "processor.h"
#include "memory.h"
#include "os.h"

/*
 * ------------------------------------------------------------
 * Processor implementation
 * ------------------------------------------------------------
 *
 * Every instruction is exactly four bytes:
 *
 *     opcode dest src1 src2
 *
 * PC is a BYTE address.
 *
 * Therefore:
 *
 *     instruction 0 -> PC = 0
 *     instruction 1 -> PC = 4
 *     instruction 2 -> PC = 8
 *     ...
 *
 * ------------------------------------------------------------
 */

/*
 * Maximum number of instructions that can be executed during
 * one call to process_instructions().
 *
 * The scheduler normally supplies TIME_SLICE instructions.
 * This additional limit is a safety net against accidental
 * misuse of the function.
 */
#define MAX_INSTRUCTIONS_PER_SLICE 10

/*
 * Instruction memory is 256 bytes.
 *
 * Four bytes are required for every instruction, so the last
 * legal instruction begins at address 252.
 */
#define INSTRUCTION_MEMORY_SIZE 256
#define INSTRUCTION_SIZE 4

/*
 * Special registers used internally by the compiler for
 * materialising memory addresses.
 *
 * These are still ordinary architectural registers.
 */
#define TEMP_ADDRESS_REGISTER 255

/*
 * ------------------------------------------------------------
 * Global processor state
 * ------------------------------------------------------------
 */

uint32_t RegisterFile[NP][INT_REGS];

uint32_t VectorRegisterFile[NP][VEC_REGS][VEC_LANES];

uint32_t PC[NP];

uint8_t opcode[NP];
uint8_t dest[NP];
uint8_t src1[NP];
uint8_t src2[NP];

int N[NP];
int Z[NP];
int C[NP];
int V[NP];

int end_of_simulation[NP];


/*
 * ------------------------------------------------------------
 * Utility functions
 * ------------------------------------------------------------
 */

static int valid_processor(int proc_id)
{
    return proc_id >= 0 && proc_id < NP;
}

static int valid_pc(uint32_t pc)
{
    /*
     * We need four bytes beginning at PC.
     */
    return pc <= INSTRUCTION_MEMORY_SIZE - INSTRUCTION_SIZE;
}


/*
 * Advance PC by exactly one instruction.
 */
static void advance_pc(int proc_id)
{
    PC[proc_id] += INSTRUCTION_SIZE;

    /*
     * If execution has moved beyond the instruction memory,
     * terminate the process safely instead of allowing fetch()
     * to access invalid memory.
     */
    if (!valid_pc(PC[proc_id])) {
        end_of_simulation[proc_id] = 1;
    }
}


/*
 * ------------------------------------------------------------
 * 32-bit memory helpers
 * ------------------------------------------------------------
 *
 * The simulator's data is byte addressable.
 *
 * A 32-bit value therefore occupies four consecutive bytes.
 * ------------------------------------------------------------
 */

static uint32_t read32(int proc_id, uint32_t logical_address)
{
    /*
     * The MMU translates logical address -> physical address.
     *
     * isFetch = 0 because this is a data access.
     */
    int physical_address =
        getPhysicallAddress(proc_id, 0, logical_address);

    if (physical_address < 0 ||
        physical_address + 3 >= MEMSIZE) {

        fprintf(stderr,
                "[Processor %d] Invalid memory read at logical "
                "address %u\n",
                proc_id,
                logical_address);

        end_of_simulation[proc_id] = 1;
        return 0;
    }

    return (uint32_t)memory[physical_address] |
           ((uint32_t)memory[physical_address + 1] << 8) |
           ((uint32_t)memory[physical_address + 2] << 16) |
           ((uint32_t)memory[physical_address + 3] << 24);
}


static int write32(int proc_id,
                   uint32_t logical_address,
                   uint32_t value)
{
    int physical_address =
        getPhysicallAddress(proc_id, 0, logical_address);

    if (physical_address < 0 ||
        physical_address + 3 >= MEMSIZE) {

        fprintf(stderr,
                "[Processor %d] Invalid memory write at logical "
                "address %u\n",
                proc_id,
                logical_address);

        end_of_simulation[proc_id] = 1;
        return 0;
    }

    memory[physical_address] =
        (uint8_t)(value & 0xFF);

    memory[physical_address + 1] =
        (uint8_t)((value >> 8) & 0xFF);

    memory[physical_address + 2] =
        (uint8_t)((value >> 16) & 0xFF);

    memory[physical_address + 3] =
        (uint8_t)((value >> 24) & 0xFF);

    return 1;
}


/*
 * ------------------------------------------------------------
 * Arithmetic flags
 * ------------------------------------------------------------
 *
 * The lab defines:
 *
 * Z = result is zero
 * N = MSB of result is one
 *
 * C for addition:
 *     unsigned result < either unsigned input
 *
 * C for subtraction:
 *     operand1 >= operand2
 *
 * V for addition:
 *     operands have same sign and result has different sign
 *
 * V for subtraction:
 *     operands have different signs and result has the sign
 *     of the second operand
 * ------------------------------------------------------------
 */

static void update_flags_add(int p,
                             uint32_t a,
                             uint32_t b,
                             uint32_t result)
{
    Z[p] = (result == 0);
    N[p] = (int)((result >> 31) & 1U);

    C[p] = (result < a) || (result < b);

    int32_t sa = (int32_t)a;
    int32_t sb = (int32_t)b;
    int32_t sr = (int32_t)result;

    V[p] =
        ((sa >= 0 && sb >= 0 && sr < 0) ||
         (sa < 0 && sb < 0 && sr >= 0));
}


static void update_flags_sub(int p,
                             uint32_t a,
                             uint32_t b,
                             uint32_t result)
{
    Z[p] = (result == 0);
    N[p] = (int)((result >> 31) & 1U);

    C[p] = (a >= b);

    int32_t sa = (int32_t)a;
    int32_t sb = (int32_t)b;
    int32_t sr = (int32_t)result;

    V[p] =
        ((sa >= 0 && sb < 0 && sr < 0) ||
         (sa < 0 && sb >= 0 && sr >= 0));
}


/*
 * ------------------------------------------------------------
 * Reset
 * ------------------------------------------------------------
 */

void reset(int proc_id)
{
    if (!valid_processor(proc_id))
        return;

    memset(RegisterFile[proc_id],
           0,
           sizeof(RegisterFile[proc_id]));

    memset(VectorRegisterFile[proc_id],
           0,
           sizeof(VectorRegisterFile[proc_id]));

    PC[proc_id] = 0;

    opcode[proc_id] = 0;
    dest[proc_id] = 0;
    src1[proc_id] = 0;
    src2[proc_id] = 0;

    N[proc_id] = 0;
    Z[proc_id] = 0;
    C[proc_id] = 0;
    V[proc_id] = 0;

    end_of_simulation[proc_id] = 0;
}


/*
 * ------------------------------------------------------------
 * Fetch
 * ------------------------------------------------------------
 *
 * Fetch reads four bytes from instruction memory.
 *
 * The lab specifically requires logical -> physical translation
 * before the memory access.
 * ------------------------------------------------------------
 */

void fetch(int proc_id)
{
    if (!valid_processor(proc_id))
        return;

    if (end_of_simulation[proc_id])
        return;

    if (!valid_pc(PC[proc_id])) {

        fprintf(stderr,
                "[Processor %d] PC %u is outside instruction memory\n",
                proc_id,
                PC[proc_id]);

        end_of_simulation[proc_id] = 1;
        return;
    }

    int physical_address =
        getPhysicallAddress(proc_id,
                            1,
                            PC[proc_id]);

    if (physical_address < 0 ||
        physical_address + 3 >= MEMSIZE) {

        fprintf(stderr,
                "[Processor %d] Invalid instruction address\n",
                proc_id);

        end_of_simulation[proc_id] = 1;
        return;
    }

    opcode[proc_id] =
        memory[physical_address];

    dest[proc_id] =
        memory[physical_address + 1];

    src1[proc_id] =
        memory[physical_address + 2];

    src2[proc_id] =
        memory[physical_address + 3];
}


/*
 * ------------------------------------------------------------
 * Decode
 * ------------------------------------------------------------
 *
 * Decode is intentionally empty at this stage, as specified
 * by the lab.
 * ------------------------------------------------------------
 */

void decode(int proc_id)
{
    (void)proc_id;
}


/*
 * ------------------------------------------------------------
 * Branch condition evaluation
 * ------------------------------------------------------------
 */

static int branch_taken(int proc_id, uint8_t op)
{
    switch (op) {

        /* BEQ */
        case 0x10:
            return Z[proc_id];

        /* BNE */
        case 0x11:
            return !Z[proc_id];

        /* BGE */
        case 0x12:
            return N[proc_id] == V[proc_id];

        /* BLT */
        case 0x13:
            return N[proc_id] != V[proc_id];

        /* BGT */
        case 0x14:
            return (!Z[proc_id]) &&
                   (N[proc_id] == V[proc_id]);

        /* BLE */
        case 0x15:
            return Z[proc_id] ||
                   (N[proc_id] != V[proc_id]);

        /* BAL */
        case 0x1E:
            return 1;

        default:
            return 0;
    }
}


/*
 * ------------------------------------------------------------
 * Branch execution
 * ------------------------------------------------------------
 *
 * src2 contains a signed 8-bit instruction offset.
 *
 * The compiler stores:
 *
 *     target_instruction - current_instruction
 *
 * Since PC is a byte address, multiply the offset by 4.
 * ------------------------------------------------------------
 */

static void execute_branch(int proc_id)
{
    int8_t offset =
        (int8_t)src2[proc_id];

    if (!branch_taken(proc_id, opcode[proc_id])) {
        advance_pc(proc_id);
        return;
    }

    int64_t current_pc =
        (int64_t)PC[proc_id];

    int64_t target_pc =
        current_pc +
        ((int64_t)offset * INSTRUCTION_SIZE);

    /*
     * The target must be a valid instruction address.
     */
    if (target_pc < 0 ||
        target_pc > INSTRUCTION_MEMORY_SIZE - INSTRUCTION_SIZE ||
        (target_pc % INSTRUCTION_SIZE) != 0) {

        fprintf(stderr,
                "[Processor %d] Invalid branch target: %lld\n",
                proc_id,
                (long long)target_pc);

        end_of_simulation[proc_id] = 1;
        return;
    }

    PC[proc_id] = (uint32_t)target_pc;
}


/*
 * ------------------------------------------------------------
 * Execute
 * ------------------------------------------------------------
 */

void execute(int proc_id)
{
    if (!valid_processor(proc_id))
        return;

    if (end_of_simulation[proc_id])
        return;

    uint32_t a;
    uint32_t b;
    uint32_t result;

    switch (opcode[proc_id]) {

        /*
         * ----------------------------------------------------
         * 0x00
         *
         * End of program.
         * ----------------------------------------------------
         */

        case 0x00:

            end_of_simulation[proc_id] = 1;
            return;


        /*
         * ----------------------------------------------------
         * Integer data movement
         *
         * 0x0F is the immediate form.
         *
         * Format:
         *
         *     0F dest 00 immediate
         * ----------------------------------------------------
         */

        case 0x0F:

            RegisterFile[proc_id][dest[proc_id]] =
                src2[proc_id];

            advance_pc(proc_id);
            break;


        /*
         * ----------------------------------------------------
         * 0x07
         *
         * Compatibility data-movement opcode.
         * ----------------------------------------------------
         */

        case 0x07:

            RegisterFile[proc_id][dest[proc_id]] =
                src2[proc_id];

            advance_pc(proc_id);
            break;


        /*
         * ----------------------------------------------------
         * Integer addition
         *
         * 0x01:
         *     xD = xA + xB
         * ----------------------------------------------------
         */

        case 0x01:

            a = RegisterFile[proc_id][src1[proc_id]];
            b = RegisterFile[proc_id][src2[proc_id]];

            result = a + b;

            RegisterFile[proc_id][dest[proc_id]] =
                result;

            update_flags_add(proc_id, a, b, result);

            advance_pc(proc_id);
            break;


        /*
         * ----------------------------------------------------
         * Integer addition with constant
         *
         * 0x09:
         *     xD = xA + constant
         * ----------------------------------------------------
         */

        case 0x09:

            a = RegisterFile[proc_id][src1[proc_id]];
            b = src2[proc_id];

            result = a + b;

            RegisterFile[proc_id][dest[proc_id]] =
                result;

            update_flags_add(proc_id, a, b, result);

            advance_pc(proc_id);
            break;


        /*
         * ----------------------------------------------------
         * Integer subtraction
         * ----------------------------------------------------
         */

        case 0x02:

            a = RegisterFile[proc_id][src1[proc_id]];
            b = RegisterFile[proc_id][src2[proc_id]];

            result = a - b;

            RegisterFile[proc_id][dest[proc_id]] =
                result;

            update_flags_sub(proc_id, a, b, result);

            advance_pc(proc_id);
            break;


        /*
         * ----------------------------------------------------
         * Integer subtraction with constant
         * ----------------------------------------------------
         */

        case 0x0A:

            a = RegisterFile[proc_id][src1[proc_id]];
            b = src2[proc_id];

            result = a - b;

            RegisterFile[proc_id][dest[proc_id]] =
                result;

            update_flags_sub(proc_id, a, b, result);

            advance_pc(proc_id);
            break;


        /*
         * ----------------------------------------------------
         * Integer multiplication
         * ----------------------------------------------------
         */

        case 0x03:

            RegisterFile[proc_id][dest[proc_id]] =
                RegisterFile[proc_id][src1[proc_id]] *
                RegisterFile[proc_id][src2[proc_id]];

            advance_pc(proc_id);
            break;


        /*
         * ----------------------------------------------------
         * Integer multiplication with constant
         * ----------------------------------------------------
         */

        case 0x0B:

            RegisterFile[proc_id][dest[proc_id]] =
                RegisterFile[proc_id][src1[proc_id]] *
                src2[proc_id];

            advance_pc(proc_id);
            break;


        /*
         * ----------------------------------------------------
         * Integer division
         * ----------------------------------------------------
         */

        case 0x04:

            b =
                RegisterFile[proc_id][src2[proc_id]];

            if (b == 0) {

                fprintf(stderr,
                        "[Processor %d] Division by zero\n",
                        proc_id);

                end_of_simulation[proc_id] = 1;
                return;
            }

            RegisterFile[proc_id][dest[proc_id]] =
                RegisterFile[proc_id][src1[proc_id]] / b;

            advance_pc(proc_id);
            break;


        /*
         * ----------------------------------------------------
         * Integer division by constant
         * ----------------------------------------------------
         */

        case 0x0C:

            b = src2[proc_id];

            if (b == 0) {

                fprintf(stderr,
                        "[Processor %d] Division by zero\n",
                        proc_id);

                end_of_simulation[proc_id] = 1;
                return;
            }

            RegisterFile[proc_id][dest[proc_id]] =
                RegisterFile[proc_id][src1[proc_id]] / b;

            advance_pc(proc_id);
            break;


        /*
         * ----------------------------------------------------
         * Integer memory read
         *
         * 05 dest address-register 00
         * ----------------------------------------------------
         */

        case 0x05:

            a =
                RegisterFile[proc_id][src1[proc_id]];

            RegisterFile[proc_id][dest[proc_id]] =
                read32(proc_id, a);

            if (!end_of_simulation[proc_id])
                advance_pc(proc_id);

            break;


        /*
         * ----------------------------------------------------
         * Integer memory write
         *
         * 06 address-register source-register 00
         * ----------------------------------------------------
         */

        case 0x06:

            a =
                RegisterFile[proc_id][dest[proc_id]];

            b =
                RegisterFile[proc_id][src1[proc_id]];

            if (write32(proc_id, a, b))
                advance_pc(proc_id);

            break;


        /*
         * ----------------------------------------------------
         * Print
         *
         * Bytecode:
         *
         * 08 00 00 register
         *
         * The lab asks for the process id and register value.
         * ----------------------------------------------------
         */

        case 0x08:
        {
            FILE *fd =
                fopen("processor.log", "a");

            if (fd != NULL) {

                fprintf(fd,
                        "Process %d: x%u : %08X\n",
                        proc_id,
                        src2[proc_id],
                        RegisterFile[proc_id][src2[proc_id]]);

                fclose(fd);
            }

            advance_pc(proc_id);
            break;
        }


        /*
         * ----------------------------------------------------
         * Vector addition
         *
         * 21 vd va vb
         * ----------------------------------------------------
         */

        case 0x21:

            for (int i = 0; i < VEC_LANES; i++) {

                VectorRegisterFile[proc_id][dest[proc_id]][i] =
                    VectorRegisterFile[proc_id][src1[proc_id]][i] +
                    VectorRegisterFile[proc_id][src2[proc_id]][i];
            }

            advance_pc(proc_id);
            break;


        /*
         * ----------------------------------------------------
         * Vector addition with scalar/constant
         *
         * 29 vd va value
         *
         * The compiler also uses this opcode when src2
         * represents an integer register. In that case the
         * compiler's current convention is interpreted here
         * as a scalar register.
         *
         * For constants 0-255, the value is used directly.
         * ----------------------------------------------------
         */

        case 0x29:

            /*
             * Constants are encoded directly.
             */
            for (int i = 0; i < VEC_LANES; i++) {

                VectorRegisterFile[proc_id][dest[proc_id]][i] =
                    VectorRegisterFile[proc_id][src1[proc_id]][i] +
                    src2[proc_id];
            }

            advance_pc(proc_id);
            break;


        /*
         * ----------------------------------------------------
         * Vector subtraction
         * ----------------------------------------------------
         */

        case 0x22:

            for (int i = 0; i < VEC_LANES; i++) {

                VectorRegisterFile[proc_id][dest[proc_id]][i] =
                    VectorRegisterFile[proc_id][src1[proc_id]][i] -
                    VectorRegisterFile[proc_id][src2[proc_id]][i];
            }

            advance_pc(proc_id);
            break;


        /*
         * ----------------------------------------------------
         * Vector subtraction with constant
         * ----------------------------------------------------
         */

        case 0x2A:

            for (int i = 0; i < VEC_LANES; i++) {

                VectorRegisterFile[proc_id][dest[proc_id]][i] =
                    VectorRegisterFile[proc_id][src1[proc_id]][i] -
                    src2[proc_id];
            }

            advance_pc(proc_id);
            break;


        /*
         * ----------------------------------------------------
         * Vector multiplication
         * ----------------------------------------------------
         */

        case 0x23:

            for (int i = 0; i < VEC_LANES; i++) {

                VectorRegisterFile[proc_id][dest[proc_id]][i] =
                    VectorRegisterFile[proc_id][src1[proc_id]][i] *
                    VectorRegisterFile[proc_id][src2[proc_id]][i];
            }

            advance_pc(proc_id);
            break;


        /*
         * ----------------------------------------------------
         * Vector multiplication with constant
         * ----------------------------------------------------
         */

        case 0x2B:

            for (int i = 0; i < VEC_LANES; i++) {

                VectorRegisterFile[proc_id][dest[proc_id]][i] =
                    VectorRegisterFile[proc_id][src1[proc_id]][i] *
                    src2[proc_id];
            }

            advance_pc(proc_id);
            break;


        /*
         * ----------------------------------------------------
         * Vector memory read
         *
         * 25 vd address-register 00
         *
         * Eight 32-bit values are loaded.
         * ----------------------------------------------------
         */

        case 0x25:
        {
            uint32_t address =
                RegisterFile[proc_id][src1[proc_id]];

            for (int i = 0; i < VEC_LANES; i++) {

                uint32_t element =
                    read32(proc_id,
                           address + (uint32_t)(i * 4));

                if (end_of_simulation[proc_id])
                    return;

                VectorRegisterFile[proc_id]
                                  [dest[proc_id]]
                                  [i] = element;
            }

            advance_pc(proc_id);
            break;
        }


        /*
         * ----------------------------------------------------
         * Vector memory write
         *
         * 26 address-register vector-register 00
         * ----------------------------------------------------
         */

        case 0x26:
        {
            uint32_t address =
                RegisterFile[proc_id][dest[proc_id]];

            for (int i = 0; i < VEC_LANES; i++) {

                uint32_t value =
                    VectorRegisterFile[proc_id]
                                      [src1[proc_id]]
                                      [i];

                if (!write32(proc_id,
                             address + (uint32_t)(i * 4),
                             value)) {
                    return;
                }
            }

            advance_pc(proc_id);
            break;
        }


        /*
         * ----------------------------------------------------
         * Branch instructions
         * ----------------------------------------------------
         */

        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x1E:

            execute_branch(proc_id);
            break;


        /*
         * ----------------------------------------------------
         * Unknown opcode
         *
         * NEVER silently treat an unknown opcode as a branch.
         *
         * The old starter implementation did this through its
         * default case, which could cause completely unrelated
         * corrupt bytecode to modify PC indefinitely.
         *
         * We terminate the task instead.
         * ----------------------------------------------------
         */

        default:

            fprintf(stderr,
                    "[Processor %d] Invalid opcode %02X at PC %u\n",
                    proc_id,
                    opcode[proc_id],
                    PC[proc_id]);

            end_of_simulation[proc_id] = 1;
            return;
    }
}


/*
 * ------------------------------------------------------------
 * Process instructions
 * ------------------------------------------------------------
 *
 * The OS calls this function with a time slice.
 *
 * It is intentionally impossible for this function to execute
 * indefinitely:
 *
 *   1. instruction_count is capped.
 *   2. Each iteration executes at most one instruction.
 *   3. end_of_simulation terminates execution immediately.
 *
 * A branch such as:
 *
 *     .loop
 *     BAL .loop
 *
 * therefore consumes one instruction and returns control to
 * the scheduler after the finite time slice.
 * ------------------------------------------------------------
 */

void process_instructions(int proc_id, int instruction_count)
{
    if (!valid_processor(proc_id))
        return;

    if (end_of_simulation[proc_id])
        return;

    if (instruction_count < 0)
        instruction_count = 0;

    if (instruction_count > MAX_INSTRUCTIONS_PER_SLICE)
        instruction_count = MAX_INSTRUCTIONS_PER_SLICE;

    for (int i = 0;
         i < instruction_count &&
         !end_of_simulation[proc_id];
         i++) {

        /*
         * fetch() validates PC before reading memory.
         */
        fetch(proc_id);

        if (end_of_simulation[proc_id])
            break;

        decode(proc_id);

        execute(proc_id);
    }

    /*
     * The lab specifies a small sleep so that real-time
     * multi-processor execution can be observed.
     */
    usleep(10);
}
