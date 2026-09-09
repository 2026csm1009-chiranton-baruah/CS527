#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "processor.h"
#include "memory.h"

/*

* ============================================================
* PROCESSOR CONFIGURATION
* ============================================================
  */

#define NUM_REGISTERS        256
#define NUM_VECTOR_REGISTERS 32

/*

* ============================================================
* PROCESSOR STATE
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

* Each simulated processor has its own register files.
  */
  static int32_t registers_file[NP][NUM_REGISTERS];

static int32_t vector_registers[NP][NUM_VECTOR_REGISTERS];

/*

* ============================================================
* PROCESSOR STATUS
* ============================================================
  */

#define STATUS_RUNNING 0
#define STATUS_HALTED  1
#define STATUS_ERROR   2

/*

* ============================================================
* VALIDATION
* ============================================================
  */

static int valid_proc(int proc_id)
{
return proc_id >= 0 &&
proc_id < NP;
}

static int valid_register(int reg)
{
return reg >= 0 &&
reg < NUM_REGISTERS;
}

static int valid_vector_register(int reg)
{
return reg >= 0 &&
reg < NUM_VECTOR_REGISTERS;
}

/*

* ============================================================
* BYTE MEMORY ACCESS
* ============================================================
  */

static int fetch_byte(int proc_id,
                      int logical_address,
                      uint8_t *value)
{
    if (!valid_proc(proc_id) || value == NULL)
        return -1;

    /* Memory subsystem handles translation and page-in. */
    return read_process_byte(proc_id, 1, logical_address, value);
}

static int read_data(int proc_id,
                     int logical_address,
                     uint8_t *value)
{
    if (!valid_proc(proc_id) || value == NULL)
        return -1;

    return read_process_byte(proc_id, 0, logical_address, value);
}

static int write_data(int proc_id,
                      int logical_address,
                      uint8_t value)
{
    if (!valid_proc(proc_id))
        return -1;

    /* Memory subsystem marks the touched page dirty. */
    return write_process_byte(proc_id, logical_address, value);
}

/*

* ============================================================
* 32-BIT DATA MEMORY ACCESS
*
* data.byte stores integers in little-endian form.
*
* Example:
*
* // ```
  08 00 00 00
  // ```
*
* represents:
*
* // ```
  8
  // ```
*
* A logical integer occupies four consecutive bytes.
* ============================================================
  */

static int read_data_word(int proc_id,
int logical_address,
int32_t *value)
{
if (!valid_proc(proc_id) ||
value == NULL) {

// ```
    return -1;
}

if (logical_address < 0 ||
    logical_address + 3 >= DATA_SIZE) {

    return -1;
}

uint8_t b0;
uint8_t b1;
uint8_t b2;
uint8_t b3;

if (read_data(proc_id,
              logical_address,
              &b0) != 0) {

    return -1;
}

if (read_data(proc_id,
              logical_address + 1,
              &b1) != 0) {

    return -1;
}

if (read_data(proc_id,
              logical_address + 2,
              &b2) != 0) {

    return -1;
}

if (read_data(proc_id,
              logical_address + 3,
              &b3) != 0) {

    return -1;
}


uint32_t word =
    ((uint32_t)b0) |
    ((uint32_t)b1 << 8) |
    ((uint32_t)b2 << 16) |
    ((uint32_t)b3 << 24);


*value =
    (int32_t)word;

return 0;
// ```

}

static int write_data_word(int proc_id,
int logical_address,
int32_t value)
{
if (!valid_proc(proc_id))
return -1;

// ```
if (logical_address < 0 ||
    logical_address + 3 >= DATA_SIZE) {

    return -1;
}


uint32_t word =
    (uint32_t)value;


if (write_data(proc_id,
               logical_address,
               (uint8_t)
               (word & 0xFF)) != 0) {

    return -1;
}


if (write_data(proc_id,
               logical_address + 1,
               (uint8_t)
               ((word >> 8) & 0xFF)) != 0) {

    return -1;
}


if (write_data(proc_id,
               logical_address + 2,
               (uint8_t)
               ((word >> 16) & 0xFF)) != 0) {

    return -1;
}


if (write_data(proc_id,
               logical_address + 3,
               (uint8_t)
               ((word >> 24) & 0xFF)) != 0) {

    return -1;
}


return 0;
// ```

}

/*

* ============================================================
* INSTRUCTION FETCH
*
* Instructions are stored as:
*
* // ```
  opcode dest src1 src2
  // ```
*
* in big-endian byte order.
* ============================================================
  */

static int fetch_instruction(int proc_id,
int pc,
uint32_t *instruction)
{
if (!valid_proc(proc_id) ||
instruction == NULL) {

// ```
    return -1;
}


uint8_t b0;
uint8_t b1;
uint8_t b2;
uint8_t b3;


if (fetch_byte(proc_id,
               pc,
               &b0) != 0 ||

    fetch_byte(proc_id,
               pc + 1,
               &b1) != 0 ||

    fetch_byte(proc_id,
               pc + 2,
               &b2) != 0 ||

    fetch_byte(proc_id,
               pc + 3,
               &b3) != 0) {

    return -1;
}


*instruction =
    ((uint32_t)b0 << 24) |
    ((uint32_t)b1 << 16) |
    ((uint32_t)b2 << 8)  |
    ((uint32_t)b3);

return 0;
// ```

}

/*

* ============================================================
* PROCESSOR CONTROL
* ============================================================
  */

static void halt_processor(int proc_id)
{
if (!valid_proc(proc_id))
return;

// ```
status[proc_id] =
    STATUS_HALTED;

end_of_simulation[proc_id] =
    1;
// ```

}

static void processor_error(int proc_id,
const char *message)
{
if (!valid_proc(proc_id))
return;

// ```
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
// ```

}

/*

* ============================================================
* PROGRAM COUNTER HELPERS
* ============================================================
  */

static int advance_pc(int proc_id)
{
PC[proc_id] += 4;

// ```
return 1;
// ```

}

/*

* Compiler branch offsets are instruction-relative.
*
* PC is byte-addressed, so:
*
* // ```
  PC = PC + offset * 4
  // ```
*
* Example:
*
* // ```
  BAL -10
  // ```
*
* means jump ten instructions backwards.
  */
  static int branch_relative(int proc_id,
  int8_t offset)
  {
  int new_pc =
  PC[proc_id] +
  ((int)offset * 4);

  if (new_pc < 0 ||
  new_pc >= INSTRUCTION_SIZE ||
  new_pc + 3 >= INSTRUCTION_SIZE) {

  // ```
   processor_error(
       proc_id,
       "branch target outside instruction memory");

   return 0;
  // ```

  }

  PC[proc_id] =
  new_pc;

  return 1;
  }

/*

* ============================================================
* EXECUTE ONE INSTRUCTION
* ============================================================
  */

static int execute_instruction(int proc_id,
uint32_t instruction)
{
if (!valid_proc(proc_id))
return 0;

// ```
uint8_t opcode =
    (uint8_t)
    ((instruction >> 24) & 0xFF);

uint8_t dest =
    (uint8_t)
    ((instruction >> 16) & 0xFF);

uint8_t src1 =
    (uint8_t)
    ((instruction >> 8) & 0xFF);

uint8_t src2 =
    (uint8_t)
    (instruction & 0xFF);


/*
 * ========================================================
 * 00 HALT
 * ========================================================
 */

if (opcode == 0x00) {

    halt_processor(proc_id);

    return 0;
}


/*
 * ========================================================
 * INTEGER REGISTER ARITHMETIC
 *
 * 01 ADD
 * 02 SUB
 * 03 MUL
 * 04 DIV
 * ========================================================
 */

if (opcode >= 0x01 &&
    opcode <= 0x04) {

    if (!valid_register(dest) ||
        !valid_register(src1) ||
        !valid_register(src2)) {

        processor_error(
            proc_id,
            "invalid integer register");

        return 0;
    }


    int32_t a =
        registers_file[proc_id][src1];

    int32_t b =
        registers_file[proc_id][src2];


    if (opcode == 0x01) {

        registers_file[proc_id][dest] =
            a + b;

    } else if (opcode == 0x02) {

        registers_file[proc_id][dest] =
            a - b;

    } else if (opcode == 0x03) {

        registers_file[proc_id][dest] =
            a * b;

    } else {

        if (b == 0) {

            processor_error(
                proc_id,
                "division by zero");

            return 0;
        }

        registers_file[proc_id][dest] =
            a / b;
    }


    AC[proc_id] =
        registers_file[proc_id][dest];

    return advance_pc(proc_id);
}


/*
 * ========================================================
 * 05 INTEGER LOAD
 *
 * xD = [xA]
 *
 * Reads a 32-bit little-endian integer.
 * ========================================================
 */

if (opcode == 0x05) {

    if (!valid_register(dest) ||
        !valid_register(src1)) {

        processor_error(
            proc_id,
            "invalid register in integer load");

        return 0;
    }


    int address =
        registers_file[proc_id][src1];


    int32_t value;

    if (read_data_word(proc_id,
                       address,
                       &value) != 0) {

        processor_error(
            proc_id,
            "integer load failed");

        return 0;
    }


    registers_file[proc_id][dest] =
        value;

    AC[proc_id] =
        value;

    return advance_pc(proc_id);
}


/*
 * ========================================================
 * 06 INTEGER STORE
 *
 * [xA] = xS
 *
 * Writes a 32-bit little-endian integer.
 * ========================================================
 */

if (opcode == 0x06) {

    if (!valid_register(dest) ||
        !valid_register(src1)) {

        processor_error(
            proc_id,
            "invalid register in integer store");

        return 0;
    }


    int address =
        registers_file[proc_id][dest];


    if (write_data_word(
            proc_id,
            address,
            registers_file[proc_id][src1]) != 0) {

        processor_error(
            proc_id,
            "integer store failed");

        return 0;
    }


    return advance_pc(proc_id);
}


/*
 * ========================================================
 * 08 PRINT
 * ========================================================
 */

if (opcode == 0x08) {

    if (!valid_register(src2)) {

        processor_error(
            proc_id,
            "invalid register in Print");

        return 0;
    }


    printf("%d\n",
           registers_file[proc_id][src2]);

    return advance_pc(proc_id);
}


/*
 * ========================================================
 * INTEGER IMMEDIATE ARITHMETIC
 *
 * 09 ADD immediate
 * 0A SUB immediate
 * 0B MUL immediate
 * 0C DIV immediate
 * ========================================================
 */

if (opcode >= 0x09 &&
    opcode <= 0x0C) {

    if (!valid_register(dest) ||
        !valid_register(src1)) {

        processor_error(
            proc_id,
            "invalid register in immediate arithmetic");

        return 0;
    }


    int32_t a =
        registers_file[proc_id][src1];

    int32_t immediate =
        (int32_t)src2;


    if (opcode == 0x09) {

        registers_file[proc_id][dest] =
            a + immediate;

    } else if (opcode == 0x0A) {

        registers_file[proc_id][dest] =
            a - immediate;

    } else if (opcode == 0x0B) {

        registers_file[proc_id][dest] =
            a * immediate;

    } else {

        if (immediate == 0) {

            processor_error(
                proc_id,
                "division by zero");

            return 0;
        }

        registers_file[proc_id][dest] =
            a / immediate;
    }


    AC[proc_id] =
        registers_file[proc_id][dest];

    return advance_pc(proc_id);
}


/*
 * ========================================================
 * 0F LOAD IMMEDIATE
 *
 * xD = constant
 * ========================================================
 */

if (opcode == 0x0F) {

    if (!valid_register(dest)) {

        processor_error(
            proc_id,
            "invalid destination register");

        return 0;
    }


    registers_file[proc_id][dest] =
        (int32_t)src2;

    AC[proc_id] =
        registers_file[proc_id][dest];

    return advance_pc(proc_id);
}


/*
 * ========================================================
 * CONDITIONAL BRANCHES
 *
 * 10 BEQ
 * 11 BNE
 * 12 BGE
 * 13 BLT
 * 14 BGT
 * 15 BLE
 *
 * Conditions are evaluated using AC.
 * ========================================================
 */

if (opcode >= 0x10 &&
    opcode <= 0x15) {

    int taken = 0;


    if (opcode == 0x10) {

        taken =
            (AC[proc_id] == 0);

    } else if (opcode == 0x11) {

        taken =
            (AC[proc_id] != 0);

    } else if (opcode == 0x12) {

        taken =
            (AC[proc_id] >= 0);

    } else if (opcode == 0x13) {

        taken =
            (AC[proc_id] < 0);

    } else if (opcode == 0x14) {

        taken =
            (AC[proc_id] > 0);

    } else {

        taken =
            (AC[proc_id] <= 0);
    }


    if (taken) {

        return branch_relative(
            proc_id,
            (int8_t)src2);
    }


    return advance_pc(proc_id);
}


/*
 * ========================================================
 * VECTOR REGISTER ARITHMETIC
 *
 * 21 ADD
 * 22 SUB
 * 23 MUL
 * ========================================================
 */

if (opcode >= 0x21 &&
    opcode <= 0x23) {

    if (!valid_vector_register(dest) ||
        !valid_vector_register(src1) ||
        !valid_vector_register(src2)) {

        processor_error(
            proc_id,
            "invalid vector register");

        return 0;
    }


    int32_t a =
        vector_registers[proc_id][src1];

    int32_t b =
        vector_registers[proc_id][src2];


    if (opcode == 0x21) {

        vector_registers[proc_id][dest] =
            a + b;

    } else if (opcode == 0x22) {

        vector_registers[proc_id][dest] =
            a - b;

    } else {

        vector_registers[proc_id][dest] =
            a * b;
    }


    return advance_pc(proc_id);
}


/*
 * ========================================================
 * 25 VECTOR LOAD
 *
 * vD = [xA]
 *
 * Reads one 32-bit integer into a vector register.
 * ========================================================
 */

if (opcode == 0x25) {

    if (!valid_vector_register(dest) ||
        !valid_register(src1)) {

        processor_error(
            proc_id,
            "invalid register in vector load");

        return 0;
    }


    int address =
        registers_file[proc_id][src1];


    int32_t value;

    if (read_data_word(proc_id,
                       address,
                       &value) != 0) {

        processor_error(
            proc_id,
            "vector load failed");

        return 0;
    }


    vector_registers[proc_id][dest] =
        value;

    return advance_pc(proc_id);
}


/*
 * ========================================================
 * 26 VECTOR STORE
 *
 * [xA] = vS
 *
 * Writes one 32-bit integer.
 * ========================================================
 */

if (opcode == 0x26) {

    if (!valid_register(dest) ||
        !valid_vector_register(src1)) {

        processor_error(
            proc_id,
            "invalid register in vector store");

        return 0;
    }


    int address =
        registers_file[proc_id][dest];


    if (write_data_word(
            proc_id,
            address,
            vector_registers[proc_id][src1]) != 0) {

        processor_error(
            proc_id,
            "vector store failed");

        return 0;
    }


    return advance_pc(proc_id);
}


/*
 * ========================================================
 * VECTOR IMMEDIATE ARITHMETIC
 *
 * 29 ADD immediate
 * 2A SUB immediate
 * 2B MUL immediate
 *
 * src2 is interpreted as an immediate constant.
 * ========================================================
 */

if (opcode >= 0x29 &&
    opcode <= 0x2B) {

    if (!valid_vector_register(dest) ||
        !valid_vector_register(src1)) {

        processor_error(
            proc_id,
            "invalid vector register");

        return 0;
    }


    int32_t a =
        vector_registers[proc_id][src1];

    int32_t immediate =
        (int32_t)src2;


    if (opcode == 0x29) {

        vector_registers[proc_id][dest] =
            a + immediate;

    } else if (opcode == 0x2A) {

        vector_registers[proc_id][dest] =
            a - immediate;

    } else {

        vector_registers[proc_id][dest] =
            a * immediate;
    }


    return advance_pc(proc_id);
}


/*
 * ========================================================
 * 1E BAL
 *
 * Unconditional instruction-relative branch.
 * ========================================================
 */

if (opcode == 0x1E) {

    return branch_relative(
        proc_id,
        (int8_t)src2);
}


/*
 * ========================================================
 * UNKNOWN OPCODE
 * ========================================================
 */

processor_error(
    proc_id,
    "unknown opcode");

return 0;
// ```

}

/*

* ============================================================
* INITIALIZATION
* ============================================================
  */

void initialize_processor(void)
{
for (int p = 0;
p < NP;
p++) {

// ```
    PC[p] = 0;

    SP[p] =
        DATA_SIZE - 1;

    AC[p] = 0;

    status[p] =
        STATUS_HALTED;

    IR[p] = 0;

    baseAddress[p] = 0;

    end_of_simulation[p] = 0;


    memset(registers_file[p],
           0,
           sizeof(registers_file[p]));


    memset(vector_registers[p],
           0,
           sizeof(vector_registers[p]));
}
// ```

}

/*

* ============================================================
* RESET ONE PROCESSOR
* ============================================================
  */

void reset(int proc_id)
{
if (!valid_proc(proc_id))
return;

// ```
PC[proc_id] = 0;

SP[proc_id] =
    DATA_SIZE - 1;

AC[proc_id] = 0;

status[proc_id] =
    STATUS_RUNNING;

IR[proc_id] = 0;

baseAddress[proc_id] = 0;

end_of_simulation[proc_id] = 0;


memset(registers_file[proc_id],
       0,
       sizeof(registers_file[proc_id]));


memset(vector_registers[proc_id],
       0,
       sizeof(vector_registers[proc_id]));
// ```

}

/*

* ============================================================
* PROCESS INSTRUCTIONS
*
* Executes at most instruction_count instructions.
* ============================================================
  */

void process_instructions(int proc_id,
int instruction_count)
{
if (!valid_proc(proc_id))
return;

// ```
if (instruction_count <= 0)
    return;

if (end_of_simulation[proc_id])
    return;


status[proc_id] =
    STATUS_RUNNING;


for (int executed = 0;
     executed < instruction_count;
     executed++) {

    if (end_of_simulation[proc_id])
        break;


    if (PC[proc_id] < 0 ||
        PC[proc_id] >= INSTRUCTION_SIZE ||
        PC[proc_id] + 3 >= INSTRUCTION_SIZE) {

        processor_error(
            proc_id,
            "program counter outside instruction memory");

        break;
    }


    uint32_t instruction;

    if (fetch_instruction(
            proc_id,
            PC[proc_id],
            &instruction) != 0) {

        processor_error(
            proc_id,
            "instruction fetch failed");

        break;
    }


    IR[proc_id] =
        (int)instruction;


    if (!execute_instruction(
            proc_id,
            instruction)) {

        break;
    }
}
// ```

}
