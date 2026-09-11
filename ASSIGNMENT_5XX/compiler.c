#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "compiler.h"

/*

* ============================================================
* COMPILER CONFIGURATION
* ============================================================
  */

#define MAX_LINES       512
#define MAX_LINE_LENGTH 256
#define MAX_LABELS      128
#define MAX_LABEL_LENGTH 64

#define MIN_CONSTANT    0
#define MAX_CONSTANT    255

/*

* x255 is reserved internally by the compiler when a constant
* memory address must be materialised before a load.
*
* Example:
*
* // ```
  x1 = [32]
  // ```
*
* becomes:
*
* // ```
  x255 = 32
  // ```
* // ```
  x1   = [x255]
  // ```
*
* Therefore source programs should not rely on x255 retaining
* its value across constant-address loads.
  */
  #define TEMP_ADDRESS_REGISTER 255

/*

* ============================================================
* LABEL TABLE
* ============================================================
  */

typedef struct {
char name[MAX_LABEL_LENGTH];

// ```
/*
 * Instruction index, not byte address.
 *
 * Instruction index:
 *
 *     0, 1, 2, ...
 *
 * Processor byte PC:
 *
 *     0, 4, 8, ...
 */
int instruction_index;
// ```

} Label;

static Label labels[MAX_LABELS];

static int label_count = 0;

/*

* ============================================================
* UTILITY FUNCTIONS
* ============================================================
  */

static char *trim(char *s)
{
while (*s &&
isspace((unsigned char)*s)) {

// ```
    s++;
}

if (*s == '\0')
    return s;

char *end =
    s + strlen(s) - 1;

while (end >= s &&
       isspace((unsigned char)*end)) {

    *end = '\0';
    end--;
}

return s;
// ```

}

static int is_valid_register(const char *s)
{
if (s == NULL ||
s[0] != 'x')
return 0;

// ```
if (s[1] == '\0')
    return 0;

for (int i = 1;
     s[i] != '\0';
     i++) {

    if (!isdigit((unsigned char)s[i]))
        return 0;
}

int n =
    atoi(s + 1);

return n >= 0 &&
       n <= 255;
// ```

}

static int is_valid_vector_register(const char *s)
{
if (s == NULL ||
s[0] != 'v')
return 0;

// ```
if (s[1] == '\0')
    return 0;

for (int i = 1;
     s[i] != '\0';
     i++) {

    if (!isdigit((unsigned char)s[i]))
        return 0;
}

int n =
    atoi(s + 1);

return n >= 0 &&
       n <= 31;
// ```

}

static int regnum(const char *s)
{
if (!is_valid_register(s))
return -1;

// ```
return atoi(s + 1);
// ```

}

static int vregnum(const char *s)
{
if (!is_valid_vector_register(s))
return -1;

// ```
return atoi(s + 1);
// ```

}

/*

* Constants are encoded in one byte.
  */
  static int parse_constant(const char *s,
  int *value)
  {
  if (s == NULL ||
  *s == '\0')
  return 0;

  for (int i = 0;
  s[i] != '\0';
  i++) {

  // ```
   if (!isdigit((unsigned char)s[i]))
       return 0;
  // ```

  }

  char *end;

  long n =
  strtol(s,
  &end,
  10);

  if (*end != '\0')
  return 0;

  if (n < MIN_CONSTANT ||
  n > MAX_CONSTANT)
  return 0;

  *value =
  (int)n;

  return 1;
  }

/*

* ============================================================
* LABEL MANAGEMENT
* ============================================================
  */

static int add_label(const char *name,
int instruction_index)
{
if (label_count >= MAX_LABELS) {

// ```
    fprintf(stderr,
            "Compiler error: too many labels\n");

    return 0;
}

if (name == NULL ||
    name[0] != '.') {

    fprintf(stderr,
            "Compiler error: invalid label '%s'\n",
            name
                ? name
                : "(null)");

    return 0;
}

if (strlen(name) >=
    MAX_LABEL_LENGTH) {

    fprintf(stderr,
            "Compiler error: label too long: %s\n",
            name);

    return 0;
}

if (name[1] == '\0') {

    fprintf(stderr,
            "Compiler error: empty label\n");

    return 0;
}

for (int i = 1;
     name[i] != '\0';
     i++) {

    if (!isalnum(
            (unsigned char)name[i])) {

        fprintf(stderr,
                "Compiler error: invalid character "
                "in label '%s'\n",
                name);

        return 0;
    }
}

for (int i = 0;
     i < label_count;
     i++) {

    if (strcmp(labels[i].name,
               name) == 0) {

        fprintf(stderr,
                "Compiler error: duplicate label '%s'\n",
                name);

        return 0;
    }
}

strcpy(labels[label_count].name,
       name);

labels[label_count]
    .instruction_index =
        instruction_index;

label_count++;

return 1;
// ```

}

static int find_label(const char *name)
{
for (int i = 0;
i < label_count;
i++) {

// ```
    if (strcmp(labels[i].name,
               name) == 0) {

        return labels[i]
            .instruction_index;
    }
}

return -1;
// ```

}

/*

* ============================================================
* BYTECODE OUTPUT
* ============================================================
  */

static void emit(FILE *out,
int opcode,
int dest,
int src1,
int src2)
{
fprintf(out,
"%02X %02X %02X %02X\n",
opcode & 0xFF,
dest & 0xFF,
src1 & 0xFF,
src2 & 0xFF);
}

static void compiler_error(int line_number,
const char *message)
{
fprintf(stderr,
"Compiler error at source line %d: %s\n",
line_number,
message);
}

/*

* ============================================================
* INSTRUCTION COUNTING
*
* This helper is used during PASS 1.
*
* It must remain consistent with PASS 2.
*
* Returns:
*
* // ```
  0 = blank/comment/label
  // ```
* // ```
  1 = normal statement
  // ```
* // ```
  2 = statement requiring temporary address materialisation
  // ```
*
* Constant memory reads are the only current statements that
* expand into two instructions:
*
* // ```
  x1 = [32]
  // ```
*
* becomes:
*
* // ```
  0F FF 00 20
  // ```
* // ```
  05 01 FF 00
  // ```
*
* The same applies to:
*
* // ```
  v1 = [32]
  // ```
* ============================================================
  */

static int instructions_for_line(char *line,
int line_number)
{
char *comment =
strchr(line, '%');

// ```
if (comment)
    *comment = '\0';

char *s =
    trim(line);

if (*s == '\0')
    return 0;


/*
 * Label.
 */
if (s[0] == '.')
    return 0;


/*
 * Print.
 */
if (strncmp(s,
            "Print ",
            6) == 0 ||
    strncmp(s,
            "print ",
            6) == 0) {

    char variable[64];

    if (sscanf(s + 6,
               "%63s",
               variable) != 1 ||
        !is_valid_register(variable)) {

        compiler_error(
            line_number,
            "Print requires an integer register");

        return -1;
    }

    return 1;
}


/*
 * Branch.
 */
{
    char branch_op[16];
    char label[64];

    if (sscanf(s,
               "%15s %63s",
               branch_op,
               label) == 2) {

        int is_branch =
            !strcmp(branch_op, "BEQ") ||
            !strcmp(branch_op, "BNE") ||
            !strcmp(branch_op, "BGE") ||
            !strcmp(branch_op, "BLT") ||
            !strcmp(branch_op, "BGT") ||
            !strcmp(branch_op, "BLE") ||
            !strcmp(branch_op, "BAL");

        if (is_branch)
            return 1;
    }
}


/*
 * Assignment.
 */
char lhs[64];
char rhs[160];

if (sscanf(s,
           " %63[^=] = %159[^\n]",
           lhs,
           rhs) != 2) {

    compiler_error(
        line_number,
        "invalid statement");

    return -1;
}

char *L =
    trim(lhs);

char *R =
    trim(rhs);


/*
 * Stores are always one instruction.
 */
if (L[0] == '[')
    return 1;


/*
 * Vector destination.
 */
if (L[0] == 'v') {

    if (!is_valid_vector_register(L)) {

        compiler_error(
            line_number,
            "invalid vector destination");

        return -1;
    }


    /*
     * Vector memory read.
     */
    if (R[0] == '[') {

        char address[64];

        if (sscanf(R,
                   "[%63[^]]]",
                   address) != 1) {

            compiler_error(
                line_number,
                "invalid vector load");

            return -1;
        }

        if (is_valid_register(address))
            return 1;

        int immediate;

        if (parse_constant(address,
                           &immediate)) {

            return 2;
        }

        compiler_error(
            line_number,
            "invalid vector memory address");

        return -1;
    }

    /*
     * All supported vector arithmetic is one instruction.
     */
    return 1;
}


/*
 * Integer destination.
 */
if (!is_valid_register(L)) {

    compiler_error(
        line_number,
        "invalid integer destination");

    return -1;
}


/*
 * Integer memory read.
 */
if (R[0] == '[') {

    char address[64];

    if (sscanf(R,
               "[%63[^]]]",
               address) != 1) {

        compiler_error(
            line_number,
            "invalid memory read");

        return -1;
    }

    if (is_valid_register(address))
        return 1;

    int immediate;

    if (parse_constant(address,
                       &immediate)) {

        return 2;
    }

    compiler_error(
        line_number,
        "invalid memory address");

    return -1;
}


/*
 * Immediate assignment and arithmetic are both one
 * instruction.
 */
return 1;
// ```

}

/*

* ============================================================
* COMPILER
* ============================================================
  */

void compile(const char *filename)
{
if (filename == NULL) {

// ```
    fprintf(stderr,
            "Compiler error: no input file\n");

    exit(EXIT_FAILURE);
}


label_count = 0;


FILE *fp =
    fopen(filename,
          "r");

if (!fp) {

    fprintf(stderr,
            "Compiler error: cannot open %s\n",
            filename);

    exit(EXIT_FAILURE);
}


char lines[MAX_LINES]
          [MAX_LINE_LENGTH];

int line_count = 0;


/*
 * Read a bounded number of source lines.
 */
while (line_count < MAX_LINES &&
       fgets(lines[line_count],
             sizeof(lines[line_count]),
             fp) != NULL) {

    line_count++;
}

fclose(fp);


/*
 * Reject files exceeding MAX_LINES.
 */
if (line_count == MAX_LINES) {

    FILE *check =
        fopen(filename,
              "r");

    if (check) {

        int c;
        int lines_seen = 0;

        while ((c = fgetc(check)) != EOF) {

            if (c == '\n')
                lines_seen++;
        }

        fclose(check);

        if (lines_seen >= MAX_LINES) {

            fprintf(stderr,
                    "Compiler error: source exceeds maximum "
                    "of %d lines\n",
                    MAX_LINES);

            exit(EXIT_FAILURE);
        }
    }
}


/*
 * ========================================================
 * PASS 1
 *
 * Resolve labels using the ACTUAL number of bytecode
 * instructions emitted by every source line.
 * ========================================================
 */

int instruction_count = 0;


for (int i = 0;
     i < line_count;
     i++) {

    char buf[MAX_LINE_LENGTH];

    strncpy(buf,
            lines[i],
            sizeof(buf) - 1);

    buf[sizeof(buf) - 1] =
        '\0';


    /*
     * Check for a label before counting instructions.
     */
    char label_buf[MAX_LINE_LENGTH];

    strncpy(label_buf,
            buf,
            sizeof(label_buf) - 1);

    label_buf[
        sizeof(label_buf) - 1] =
            '\0';

    char *comment =
        strchr(label_buf,
               '%');

    if (comment)
        *comment = '\0';

    char *s =
        trim(label_buf);

    if (*s == '\0')
        continue;


    if (s[0] == '.') {

        if (!add_label(
                s,
                instruction_count)) {

            exit(EXIT_FAILURE);
        }

        continue;
    }


    /*
     * Count the same number of instructions that PASS 2
     * will emit.
     */
    int produced =
        instructions_for_line(
            buf,
            i + 1);

    if (produced < 0)
        exit(EXIT_FAILURE);

    instruction_count +=
        produced;
}


/*
 * One HALT instruction is appended after the source.
 */
if (instruction_count + 1 >
    256) {

    fprintf(stderr,
            "Compiler error: program exceeds the "
            "maximum supported instruction count\n");

    exit(EXIT_FAILURE);
}


FILE *out =
    fopen("program.byte",
          "w");

if (!out) {

    fprintf(stderr,
            "Compiler error: cannot create program.byte\n");

    exit(EXIT_FAILURE);
}


/*
 * ========================================================
 * PASS 2
 *
 * Generate bytecode.
 * ========================================================
 */

int pc = 0;


for (int line_number = 0;
     line_number < line_count;
     line_number++) {

    char buf[MAX_LINE_LENGTH];

    strncpy(buf,
            lines[line_number],
            sizeof(buf) - 1);

    buf[sizeof(buf) - 1] =
        '\0';


    char *comment =
        strchr(buf,
               '%');

    if (comment)
        *comment = '\0';


    char *s =
        trim(buf);

    if (*s == '\0')
        continue;

    if (s[0] == '.')
        continue;


    /*
     * ====================================================
     * PRINT
     * ====================================================
     */

    if (strncmp(s,
                "Print ",
                6) == 0 ||
        strncmp(s,
                "print ",
                6) == 0) {

        char variable[64];

        if (sscanf(s + 6,
                   "%63s",
                   variable) != 1 ||
            !is_valid_register(variable)) {

            compiler_error(
                line_number + 1,
                "Print requires an integer register");

            fclose(out);
            exit(EXIT_FAILURE);
        }

        emit(out,
             0x08,
             0,
             0,
             regnum(variable));

        pc++;

        continue;
    }


    /*
     * ====================================================
     * BRANCHES
     * ====================================================
     */

    {
        char branch_op[16];
        char label[64];

        if (sscanf(s,
                   "%15s %63s",
                   branch_op,
                   label) == 2) {

            int is_branch =
                !strcmp(branch_op, "BEQ") ||
                !strcmp(branch_op, "BNE") ||
                !strcmp(branch_op, "BGE") ||
                !strcmp(branch_op, "BLT") ||
                !strcmp(branch_op, "BGT") ||
                !strcmp(branch_op, "BLE") ||
                !strcmp(branch_op, "BAL");

            if (is_branch) {

                int target =
                    find_label(label);

                if (target < 0) {

                    compiler_error(
                        line_number + 1,
                        "branch target label not found");

                    fclose(out);
                    exit(EXIT_FAILURE);
                }


                /*
                 * Both compiler and patched processor use
                 * instruction-relative offsets:
                 *
                 *     offset =
                 *         target_instruction
                 *         - current_instruction
                 */
                int offset =
                    target - pc;


                if (offset < -128 ||
                    offset > 127) {

                    compiler_error(
                        line_number + 1,
                        "branch offset exceeds "
                        "signed 8-bit range");

                    fclose(out);
                    exit(EXIT_FAILURE);
                }


                int opcode_value;

                if (!strcmp(branch_op,
                            "BEQ"))

                    opcode_value = 0x10;

                else if (!strcmp(branch_op,
                                 "BNE"))

                    opcode_value = 0x11;

                else if (!strcmp(branch_op,
                                 "BGE"))

                    opcode_value = 0x12;

                else if (!strcmp(branch_op,
                                 "BLT"))

                    opcode_value = 0x13;

                else if (!strcmp(branch_op,
                                 "BGT"))

                    opcode_value = 0x14;

                else if (!strcmp(branch_op,
                                 "BLE"))

                    opcode_value = 0x15;

                else

                    opcode_value = 0x1E;


                emit(out,
                     opcode_value,
                     0,
                     0,
                     (unsigned char)
                     (int8_t)offset);

                pc++;

                continue;
            }
        }
    }


    /*
     * ====================================================
     * ASSIGNMENT
     * ====================================================
     */

    char lhs[64];
    char rhs[160];

    if (sscanf(s,
               " %63[^=] = %159[^\n]",
               lhs,
               rhs) != 2) {

        compiler_error(
            line_number + 1,
            "invalid statement");

        fclose(out);
        exit(EXIT_FAILURE);
    }


    char *L =
        trim(lhs);

    char *R =
        trim(rhs);


    /*
     * ====================================================
     * VECTOR STORE
     *
     * [xN] = vN
     * ====================================================
     */

    if (L[0] == '[' &&
        R[0] == 'v') {

        char address[64];

        if (sscanf(L,
                   "[%63[^]]]",
                   address) != 1 ||
            !is_valid_register(address) ||
            !is_valid_vector_register(R)) {

            compiler_error(
                line_number + 1,
                "invalid vector store");

            fclose(out);
            exit(EXIT_FAILURE);
        }

        emit(out,
             0x26,
             regnum(address),
             vregnum(R),
             0);

        pc++;

        continue;
    }


    /*
     * ====================================================
     * INTEGER STORE
     *
     * [xN] = xN
     * ====================================================
     */

    if (L[0] == '[') {

        char address[64];

        if (sscanf(L,
                   "[%63[^]]]",
                   address) != 1 ||
            !is_valid_register(address) ||
            !is_valid_register(R)) {

            compiler_error(
                line_number + 1,
                "invalid integer store");

            fclose(out);
            exit(EXIT_FAILURE);
        }

        emit(out,
             0x06,
             regnum(address),
             regnum(R),
             0);

        pc++;

        continue;
    }


    /*
     * ====================================================
     * VECTOR DESTINATION
     * ====================================================
     */

    if (L[0] == 'v') {

        int destination =
            vregnum(L);

        if (destination < 0) {

            compiler_error(
                line_number + 1,
                "invalid vector destination");

            fclose(out);
            exit(EXIT_FAILURE);
        }


        /*
         * ------------------------------------------------
         * VECTOR LOAD
         * ------------------------------------------------
         */

        if (R[0] == '[') {

            char address[64];

            if (sscanf(R,
                       "[%63[^]]]",
                       address) != 1) {

                compiler_error(
                    line_number + 1,
                    "invalid vector load");

                fclose(out);
                exit(EXIT_FAILURE);
            }


            if (is_valid_register(address)) {

                emit(out,
                     0x25,
                     destination,
                     regnum(address),
                     0);

                pc++;

                continue;
            }


            int immediate;

            if (parse_constant(
                    address,
                    &immediate)) {

                emit(out,
                     0x0F,
                     TEMP_ADDRESS_REGISTER,
                     0,
                     immediate);

                emit(out,
                     0x25,
                     destination,
                     TEMP_ADDRESS_REGISTER,
                     0);

                pc += 2;

                continue;
            }


            compiler_error(
                line_number + 1,
                "invalid vector memory address");

            fclose(out);
            exit(EXIT_FAILURE);
        }


        /*
         * ------------------------------------------------
         * VECTOR ARITHMETIC
         * ------------------------------------------------
         */

        char a[64];
        char operation[8];
        char b[64];

        if (sscanf(R,
                   "%63s %7s %63s",
                   a,
                   operation,
                   b) != 3) {

            compiler_error(
                line_number + 1,
                "invalid vector expression");

            fclose(out);
            exit(EXIT_FAILURE);
        }


        if (strlen(operation) != 1 ||
            (operation[0] != '+' &&
             operation[0] != '-' &&
             operation[0] != '*')) {

            compiler_error(
                line_number + 1,
                "invalid vector operator");

            fclose(out);
            exit(EXIT_FAILURE);
        }


        int va =
            vregnum(a);

        if (va < 0) {

            compiler_error(
                line_number + 1,
                "first vector operand must be "
                "a vector register");

            fclose(out);
            exit(EXIT_FAILURE);
        }


        /*
         * Vector + vector.
         */
        if (is_valid_vector_register(b)) {

            int vb =
                vregnum(b);

            int opcode_value;

            if (operation[0] == '+')
                opcode_value = 0x21;

            else if (operation[0] == '-')
                opcode_value = 0x22;

            else
                opcode_value = 0x23;


            emit(out,
                 opcode_value,
                 destination,
                 va,
                 vb);

            pc++;

            continue;
        }


        /*
         * Vector + constant.
         */
        int immediate;

        if (parse_constant(
                b,
                &immediate)) {

            int opcode_value;

            if (operation[0] == '+')
                opcode_value = 0x29;

            else if (operation[0] == '-')
                opcode_value = 0x2A;

            else
                opcode_value = 0x2B;


            emit(out,
                 opcode_value,
                 destination,
                 va,
                 immediate);

            pc++;

            continue;
        }


        /*
         * Preserve the original bytecode convention for
         * vector/scalar-register arithmetic.
         *
         * Note: the ISA encoding itself is ambiguous because
         * the same opcodes are also used for immediates.
         */
        if (is_valid_register(b)) {

            int scalar =
                regnum(b);

            int opcode_value;

            if (operation[0] == '+')
                opcode_value = 0x29;

            else if (operation[0] == '-')
                opcode_value = 0x2A;

            else
                opcode_value = 0x2B;


            emit(out,
                 opcode_value,
                 destination,
                 va,
                 scalar);

            pc++;

            continue;
        }


        compiler_error(
            line_number + 1,
            "invalid second vector operand");

        fclose(out);
        exit(EXIT_FAILURE);
    }


    /*
     * ====================================================
     * INTEGER DESTINATION
     * ====================================================
     */

    int destination =
        regnum(L);

    if (destination < 0) {

        compiler_error(
            line_number + 1,
            "invalid integer destination");

        fclose(out);
        exit(EXIT_FAILURE);
    }


    /*
     * ----------------------------------------------------
     * INTEGER MEMORY READ
     *
     * x1 = [x2]
     * x1 = [32]
     * ----------------------------------------------------
     */

    if (R[0] == '[') {

        char address[64];

        if (sscanf(R,
                   "[%63[^]]]",
                   address) != 1) {

            compiler_error(
                line_number + 1,
                "invalid memory read");

            fclose(out);
            exit(EXIT_FAILURE);
        }


        if (is_valid_register(address)) {

            emit(out,
                 0x05,
                 destination,
                 regnum(address),
                 0);

            pc++;

            continue;
        }


        int immediate;

        if (parse_constant(
                address,
                &immediate)) {

            emit(out,
                 0x0F,
                 TEMP_ADDRESS_REGISTER,
                 0,
                 immediate);

            emit(out,
                 0x05,
                 destination,
                 TEMP_ADDRESS_REGISTER,
                 0);

            pc += 2;

            continue;
        }


        compiler_error(
            line_number + 1,
            "invalid memory address");

        fclose(out);
        exit(EXIT_FAILURE);
    }


    /*
     * ----------------------------------------------------
     * IMMEDIATE DATA MOVEMENT
     *
     * x1 = 25
     * ----------------------------------------------------
     */

    {
        int immediate;

        if (parse_constant(
                R,
                &immediate)) {

            emit(out,
                 0x0F,
                 destination,
                 0,
                 immediate);

            pc++;

            continue;
        }
    }


    /*
     * ----------------------------------------------------
     * INTEGER ARITHMETIC
     *
     * x1 = x2 + x3
     * x1 = x2 + 10
     * ----------------------------------------------------
     */

    {
        char a[64];
        char operation[8];
        char b[64];

        if (sscanf(R,
                   "%63s %7s %63s",
                   a,
                   operation,
                   b) != 3) {

            compiler_error(
                line_number + 1,
                "invalid integer expression");

            fclose(out);
            exit(EXIT_FAILURE);
        }


        if (strlen(operation) != 1 ||
            (operation[0] != '+' &&
             operation[0] != '-' &&
             operation[0] != '*' &&
             operation[0] != '/')) {

            compiler_error(
                line_number + 1,
                "invalid integer operator");

            fclose(out);
            exit(EXIT_FAILURE);
        }


        int operand1 =
            regnum(a);

        if (operand1 < 0) {

            compiler_error(
                line_number + 1,
                "first integer operand must be "
                "a register");

            fclose(out);
            exit(EXIT_FAILURE);
        }


        /*
         * Arithmetic with a constant.
         */
        int immediate;

        if (parse_constant(
                b,
                &immediate)) {

            int opcode_value;

            if (operation[0] == '+')
                opcode_value = 0x09;

            else if (operation[0] == '-')
                opcode_value = 0x0A;

            else if (operation[0] == '*')
                opcode_value = 0x0B;

            else
                opcode_value = 0x0C;


            emit(out,
                 opcode_value,
                 destination,
                 operand1,
                 immediate);

            pc++;

            continue;
        }


        /*
         * Arithmetic with another register.
         */
        int operand2 =
            regnum(b);

        if (operand2 < 0) {

            compiler_error(
                line_number + 1,
                "invalid second integer operand");

            fclose(out);
            exit(EXIT_FAILURE);
        }


        int opcode_value;

        if (operation[0] == '+')
            opcode_value = 0x01;

        else if (operation[0] == '-')
            opcode_value = 0x02;

        else if (operation[0] == '*')
            opcode_value = 0x03;

        else
            opcode_value = 0x04;


        emit(out,
             opcode_value,
             destination,
             operand1,
             operand2);

        pc++;
    }
}


/*
 * ========================================================
 * HALT
 *
 * Every normally compiled program terminates with opcode 00.
 * ========================================================
 */

emit(out,
     0x00,
     0,
     0,
     0);


fclose(out);
// ```

}
