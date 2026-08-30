#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "processor.h"
#include "memory.h"


/*
 * ============================================================
 * Processor state
 * ============================================================
 */

int PC[NP];
int SP[NP];

int AC[NP];

int status[NP];

int IR[NP];

int baseAddress[NP];

int end_of_simulation[NP];


/*
 * ============================================================
 * Processor status flags
 * ============================================================
 *
 * These names are intentionally kept simple so the processor
 * can represent the architectural state without introducing
 * unnecessary abstractions.
 * ============================================================
 */

#define STATUS_RUNNING    0
#define STATUS_HALTED     1
#define STATUS_ERROR      2


/*
 * ============================================================
 * Internal helpers
 * ============================================================
 */

static int valid_proc(int proc_id)
{
    return proc_id >= 0 && proc_id < NP;
}


/*
 * ============================================================
 * Read one instruction byte through physical memory
 * ============================================================
 */

static int fetch_byte(int proc_id,
                      int logical_address,
                      uint8_t *value)
{
    if (!valid_proc(proc_id))
        return -1;

    if (value == NULL)
        return -1;

    int physical_address =
        getPhysicallAddress(proc_id,
                            1,
                            logical_address);

    if (physical_address < 0)
        return -1;

    return read_physical_byte(physical_address,
                              value);
}


/*
 * ============================================================
 * Fetch four-byte instruction
 * ============================================================
 *
 * Instructions are stored as four bytes.
 *
 * Byte order:
 *
 *     [opcode][operand high][operand][operand low]
 *
 * The exact encoding is defined by compiler.c.
 *
 * This processor treats the four bytes as a 32-bit instruction
 * value in big-endian order.
 * ============================================================
 */

static int fetch_instruction(int proc_id,
                             int pc,
                             uint32_t *instruction)
{
    if (!valid_proc(proc_id))
        return -1;

    if (instruction == NULL)
        return -1;

    uint8_t b0;
    uint8_t b1;
    uint8_t b2;
    uint8_t b3;

    if (fetch_byte(proc_id,
                   pc,
                   &b0) != 0)
        return -1;

    if (fetch_byte(proc_id,
                   pc + 1,
                   &b1) != 0)
        return -1;

    if (fetch_byte(proc_id,
                   pc + 2,
                   &b2) != 0)
        return -1;

    if (fetch_byte(proc_id,
                   pc + 3,
                   &b3) != 0)
        return -1;

    *instruction =
        ((uint32_t)b0 << 24) |
        ((uint32_t)b1 << 16) |
        ((uint32_t)b2 << 8)  |
        ((uint32_t)b3);

    return 0;
}


/*
 * ============================================================
 * Data-memory read
 * ============================================================
 */

static int read_data(int proc_id,
                     int logical_address,
                     uint8_t *value)
{
    if (!valid_proc(proc_id))
        return -1;

    if (value == NULL)
        return -1;

    int physical_address =
        getPhysicallAddress(proc_id,
                            0,
                            logical_address);

    if (physical_address < 0)
        return -1;

    return read_physical_byte(physical_address,
                              value);
}


/*
 * ============================================================
 * Data-memory write
 * ============================================================
 */

static int write_data(int proc_id,
                      int logical_address,
                      uint8_t value)
{
    if (!valid_proc(proc_id))
        return -1;

    int physical_address =
        getPhysicallAddress(proc_id,
                            0,
                            logical_address);

    if (physical_address < 0)
        return -1;

    return write_physical_byte(physical_address,
                               value);
}


/*
 * ============================================================
 * Convert four bytes to signed 32-bit value
 * ============================================================
 */

static int32_t decode_signed32(uint32_t value)
{
    return (int32_t)value;
}


/*
 * ============================================================
 * Processor initialization
 * ============================================================
 */

void initialize_processor(void)
{
    for (int p = 0;
         p < NP;
         p++) {

        PC[p] = 0;
        SP[p] = DATA_SIZE - 1;

        AC[p] = 0;

        status[p] =
            STATUS_HALTED;

        IR[p] = 0;

        baseAddress[p] = 0;

        end_of_simulation[p] = 0;
    }
}


/*
 * ============================================================
 * Reset one processor
 * ============================================================
 */

void reset(int proc_id)
{
    if (!valid_proc(proc_id))
        return;

    PC[proc_id] = 0;

    /*
     * Stack pointer starts at the top of logical data memory.
     */
    SP[proc_id] =
        DATA_SIZE - 1;

    AC[proc_id] = 0;

    status[proc_id] =
        STATUS_RUNNING;

    IR[proc_id] = 0;

    baseAddress[proc_id] = 0;

    end_of_simulation[proc_id] = 0;
}


/*
 * ============================================================
 * Halt processor
 * ============================================================
 */

static void halt_processor(int proc_id)
{
    if (!valid_proc(proc_id))
        return;

    status[proc_id] =
        STATUS_HALTED;

    end_of_simulation[proc_id] =
        1;
}


/*
 * ============================================================
 * Processor error
 * ============================================================
 */

static void processor_error(int proc_id,
                            const char *message)
{
    if (!valid_proc(proc_id))
        return;

    fprintf(stderr,
            "[Processor %d] ERROR: %s\n",
            proc_id,
            message != NULL
                ? message
                : "unknown processor error");

    status[proc_id] =
        STATUS_ERROR;

    end_of_simulation[proc_id] =
        1;
}


/*
 * ============================================================
 * Execute one instruction
 * ============================================================
 *
 * Returns:
 *
 *     1 = instruction executed and processor may continue
 *     0 = processor stopped
 *
 * The opcode occupies the most significant byte.
 *
 * The remaining 24 bits form the operand.
 *
 * This keeps the instruction representation compatible with
 * four-byte program.byte instructions.
 * ============================================================
 */

static int execute_instruction(int proc_id,
                               uint32_t instruction)
{
    if (!valid_proc(proc_id))
        return 0;

    uint8_t opcode =
        (uint8_t)((instruction >> 24) & 0xFF);

    uint32_t operand =
        instruction & 0x00FFFFFF;


    /*
     * --------------------------------------------------------
     * STOP / HALT
     * --------------------------------------------------------
     *
     * Opcode value is taken from the processor/compiler
     * instruction definition.
     *
     * The common halt value is 0.
     */
    if (opcode == 0x00) {

        halt_processor(proc_id);

        return 0;
    }


    /*
     * --------------------------------------------------------
     * LDC
     * --------------------------------------------------------
     *
     * Load constant into accumulator.
     */
    if (opcode == 0x01) {

        AC[proc_id] =
            decode_signed32(operand);

        PC[proc_id] += 4;

        return 1;
    }


    /*
     * --------------------------------------------------------
     * LD
     * --------------------------------------------------------
     *
     * Load a byte from logical data memory.
     */
    if (opcode == 0x02) {

        if (operand >= DATA_SIZE) {

            processor_error(proc_id,
                            "LD address outside data memory");

            return 0;
        }

        uint8_t value;

        if (read_data(proc_id,
                      (int)operand,
                      &value) != 0) {

            processor_error(proc_id,
                            "LD physical address translation failed");

            return 0;
        }

        AC[proc_id] =
            (int32_t)value;

        PC[proc_id] += 4;

        return 1;
    }


    /*
     * --------------------------------------------------------
     * ST
     * --------------------------------------------------------
     *
     * Store accumulator into logical data memory.
     */
    if (opcode == 0x03) {

        if (operand >= DATA_SIZE) {

            processor_error(proc_id,
                            "ST address outside data memory");

            return 0;
        }

        if (write_data(proc_id,
                       (int)operand,
                       (uint8_t)(AC[proc_id] & 0xFF)) != 0) {

            processor_error(proc_id,
                            "ST physical address translation failed");

            return 0;
        }

        PC[proc_id] += 4;

        return 1;
    }


    /*
     * --------------------------------------------------------
     * ADD
     * --------------------------------------------------------
     *
     * AC = AC + memory[address]
     */
    if (opcode == 0x04) {

        if (operand >= DATA_SIZE) {

            processor_error(proc_id,
                            "ADD address outside data memory");

            return 0;
        }

        uint8_t value;

        if (read_data(proc_id,
                      (int)operand,
                      &value) != 0) {

            processor_error(proc_id,
                            "ADD physical address translation failed");

            return 0;
        }

        AC[proc_id] +=
            (int32_t)value;

        PC[proc_id] += 4;

        return 1;
    }


    /*
     * --------------------------------------------------------
     * SUB
     * --------------------------------------------------------
     */

    if (opcode == 0x05) {

        if (operand >= DATA_SIZE) {

            processor_error(proc_id,
                            "SUB address outside data memory");

            return 0;
        }

        uint8_t value;

        if (read_data(proc_id,
                      (int)operand,
                      &value) != 0) {

            processor_error(proc_id,
                            "SUB physical address translation failed");

            return 0;
        }

        AC[proc_id] -=
            (int32_t)value;

        PC[proc_id] += 4;

        return 1;
    }


    /*
     * --------------------------------------------------------
     * BAL
     * --------------------------------------------------------
     *
     * Branch-and-link.
     *
     * The target is a logical instruction address.
     *
     * The important scheduler property is that BAL itself
     * executes exactly once and returns to the scheduler after
     * the time slice expires.
     */
    if (opcode == 0x06) {

        if (operand >= INSTRUCTION_SIZE) {

            processor_error(proc_id,
                            "BAL target outside instruction memory");

            return 0;
        }

        /*
         * Store return address.
         *
         * Stack bounds are checked before use.
         */
        if (SP[proc_id] < 0 ||
            SP[proc_id] >= DATA_SIZE) {

            processor_error(proc_id,
                            "invalid stack pointer");

            return 0;
        }

        if (write_data(proc_id,
                       SP[proc_id],
                       (uint8_t)((PC[proc_id] + 4) & 0xFF)) != 0) {

            processor_error(proc_id,
                            "BAL stack write failed");

            return 0;
        }

        PC[proc_id] =
            (int)operand;

        return 1;
    }


    /*
     * --------------------------------------------------------
     * Unknown opcode
     * --------------------------------------------------------
     */

    processor_error(proc_id,
                    "unknown opcode");

    return 0;
}


/*
 * ============================================================
 * Process instructions
 * ============================================================
 *
 * This function is intentionally bounded.
 *
 * instruction_count is the maximum number of instructions that
 * can execute during this invocation.
 *
 * Therefore:
 *
 *     process_instructions(p, 10)
 *
 * can execute at most 10 instructions.
 *
 * Even an infinite user-program loop cannot make this function
 * execute indefinitely.
 * ============================================================
 */

void process_instructions(int proc_id,
                          int instruction_count)
{
    if (!valid_proc(proc_id))
        return;

    if (instruction_count <= 0)
        return;

    if (end_of_simulation[proc_id])
        return;


    status[proc_id] =
        STATUS_RUNNING;


    for (int executed = 0;
         executed < instruction_count;
         executed++) {

        /*
         * A HALT or processor error immediately terminates
         * this invocation.
         */
        if (end_of_simulation[proc_id])
            break;


        /*
         * PC must point to a four-byte instruction.
         */
        if (PC[proc_id] < 0 ||
            PC[proc_id] >= INSTRUCTION_SIZE ||
            PC[proc_id] + 3 >= INSTRUCTION_SIZE) {

            processor_error(
                proc_id,
                "program counter outside instruction memory");

            break;
        }


        /*
         * Fetch through the virtual/logical -> physical
         * translation layer.
         */
        uint32_t instruction;

        if (fetch_instruction(proc_id,
                              PC[proc_id],
                              &instruction) != 0) {

            processor_error(
                proc_id,
                "instruction fetch failed");

            break;
        }


        /*
         * Keep the current instruction visible in IR.
         */
        IR[proc_id] =
            (int)instruction;


        /*
         * Execute exactly one instruction.
         */
        if (!execute_instruction(proc_id,
                                 instruction)) {

            break;
        }
    }
}
