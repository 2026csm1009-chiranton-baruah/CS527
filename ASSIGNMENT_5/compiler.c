#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "compiler.h"

#define MAX_LINES       512
#define MAX_LINE_LENGTH 256
#define MAX_LABELS      128
#define MAX_LABEL_LENGTH 64

#define MIN_CONSTANT    0
#define MAX_CONSTANT    255

typedef struct {
    char name[MAX_LABEL_LENGTH];
    int instruction_index;
} Label;

static Label labels[MAX_LABELS];
static int label_count = 0;

/* ============================================================
 * Utility functions
 * ============================================================ */

static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s))
        s++;

    if (*s == '\0')
        return s;

    char *end = s + strlen(s) - 1;

    while (end >= s && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }

    return s;
}

static int is_valid_register(const char *s)
{
    if (s == NULL || s[0] != 'x')
        return 0;

    if (s[1] == '\0')
        return 0;

    for (int i = 1; s[i] != '\0'; i++) {
        if (!isdigit((unsigned char)s[i]))
            return 0;
    }

    int n = atoi(s + 1);

    return n >= 0 && n <= 255;
}

static int is_valid_vector_register(const char *s)
{
    if (s == NULL || s[0] != 'v')
        return 0;

    if (s[1] == '\0')
        return 0;

    for (int i = 1; s[i] != '\0'; i++) {
        if (!isdigit((unsigned char)s[i]))
            return 0;
    }

    int n = atoi(s + 1);

    return n >= 0 && n <= 31;
}

static int regnum(const char *s)
{
    if (!is_valid_register(s))
        return -1;

    return atoi(s + 1);
}

static int vregnum(const char *s)
{
    if (!is_valid_vector_register(s))
        return -1;

    return atoi(s + 1);
}

static int parse_constant(const char *s, int *value)
{
    if (s == NULL || *s == '\0')
        return 0;

    for (int i = 0; s[i] != '\0'; i++) {
        if (!isdigit((unsigned char)s[i]))
            return 0;
    }

    char *end;
    long n = strtol(s, &end, 10);

    if (*end != '\0')
        return 0;

    if (n < MIN_CONSTANT || n > MAX_CONSTANT)
        return 0;

    *value = (int)n;
    return 1;
}

static int add_label(const char *name, int instruction_index)
{
    if (label_count >= MAX_LABELS) {
        fprintf(stderr, "Compiler error: too many labels\n");
        return 0;
    }

    if (name == NULL || name[0] != '.') {
        fprintf(stderr, "Compiler error: invalid label '%s'\n",
                name ? name : "(null)");
        return 0;
    }

    if (strlen(name) >= MAX_LABEL_LENGTH) {
        fprintf(stderr, "Compiler error: label too long: %s\n", name);
        return 0;
    }

    if (name[1] == '\0') {
        fprintf(stderr, "Compiler error: empty label\n");
        return 0;
    }

    for (int i = 1; name[i] != '\0'; i++) {
        if (!isalnum((unsigned char)name[i])) {
            fprintf(stderr,
                    "Compiler error: invalid character in label '%s'\n",
                    name);
            return 0;
        }
    }

    for (int i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, name) == 0) {
            fprintf(stderr, "Compiler error: duplicate label '%s'\n",
                    name);
            return 0;
        }
    }

    strcpy(labels[label_count].name, name);
    labels[label_count].instruction_index = instruction_index;
    label_count++;

    return 1;
}

static int find_label(const char *name)
{
    for (int i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, name) == 0)
            return labels[i].instruction_index;
    }

    return -1;
}

static void emit(FILE *out, int opcode, int dest, int src1, int src2)
{
    fprintf(out,
            "%02X %02X %02X %02X\n",
            opcode & 0xFF,
            dest & 0xFF,
            src1 & 0xFF,
            src2 & 0xFF);
}

static void compiler_error(int line_number, const char *message)
{
    fprintf(stderr,
            "Compiler error at source line %d: %s\n",
            line_number,
            message);
}

/* ============================================================
 * Compiler
 * ============================================================ */

void compile(const char *filename)
{
    if (filename == NULL) {
        fprintf(stderr, "Compiler error: no input file\n");
        exit(EXIT_FAILURE);
    }

    label_count = 0;

    FILE *fp = fopen(filename, "r");

    if (!fp) {
        fprintf(stderr,
                "Compiler error: cannot open %s\n",
                filename);
        exit(EXIT_FAILURE);
    }

    char lines[MAX_LINES][MAX_LINE_LENGTH];
    int line_count = 0;

    /*
     * Read a bounded number of source lines.
     * This prevents the compiler from consuming unlimited input.
     */
    while (line_count < MAX_LINES &&
           fgets(lines[line_count],
                 sizeof(lines[line_count]),
                 fp) != NULL) {
        line_count++;
    }

    fclose(fp);

    /*
     * If there is still another line, the source exceeds
     * the supported maximum.
     */
    if (line_count == MAX_LINES) {
        FILE *check = fopen(filename, "r");

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

    /* ========================================================
     * PASS 1
     *
     * Find all labels and determine their instruction indices.
     * ======================================================== */

    int instruction_count = 0;

    for (int i = 0; i < line_count; i++) {

        char *comment = strchr(lines[i], '%');

        if (comment)
            *comment = '\0';

        char *s = trim(lines[i]);

        if (*s == '\0')
            continue;

        /*
         * Labels must be the first non-whitespace character
         * according to the language specification.
         */
        if (s[0] == '.') {

            if (!add_label(s, instruction_count)) {
                exit(EXIT_FAILURE);
            }

            continue;
        }

        instruction_count++;
    }

    FILE *out = fopen("program.byte", "w");

    if (!out) {
        fprintf(stderr,
                "Compiler error: cannot create program.byte\n");
        exit(EXIT_FAILURE);
    }

    /* ========================================================
     * PASS 2
     *
     * Generate bytecode.
     * ======================================================== */

    int pc = 0;

    for (int line_number = 0;
         line_number < line_count;
         line_number++) {

        char buf[MAX_LINE_LENGTH];

        strncpy(buf, lines[line_number], sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        char *comment = strchr(buf, '%');

        if (comment)
            *comment = '\0';

        char *s = trim(buf);

        if (*s == '\0')
            continue;

        if (s[0] == '.')
            continue;

        /* ====================================================
         * PRINT
         *
         * Print xN
         *
         * Opcode:
         * 08 00 00 register
         * ==================================================== */

        if (strncmp(s, "Print ", 6) == 0 ||
            strncmp(s, "print ", 6) == 0) {

            char variable[64];

            if (sscanf(s + 6, "%63s", variable) != 1 ||
                !is_valid_register(variable)) {

                compiler_error(line_number + 1,
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

        /* ====================================================
         * BRANCH
         *
         * BEQ .label
         * BNE .label
         * BGE .label
         * BLT .label
         * BGT .label
         * BLE .label
         * BAL .label
         *
         * Bytecode:
         * opcode 00 00 offset
         * ==================================================== */

        {
            char branch_op[16];
            char label[64];

            if (sscanf(s, "%15s %63s",
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

                    int target = find_label(label);

                    if (target < 0) {
                        compiler_error(line_number + 1,
                                       "branch target label not found");
                        fclose(out);
                        exit(EXIT_FAILURE);
                    }

                    /*
                     * The bytecode stores the relative instruction
                     * offset. Processor PC is advanced in 4-byte
                     * instruction units.
                     */
                    int offset = target - pc;

                    /*
                     * src2 is one byte, so the relative offset must
                     * fit in a signed byte.
                     */
                    if (offset < -128 || offset > 127) {
                        compiler_error(line_number + 1,
                                       "branch offset exceeds "
                                       "signed 8-bit range");
                        fclose(out);
                        exit(EXIT_FAILURE);
                    }

                    int opcode_value = 0x1E;

                    if (!strcmp(branch_op, "BEQ"))
                        opcode_value = 0x10;
                    else if (!strcmp(branch_op, "BNE"))
                        opcode_value = 0x11;
                    else if (!strcmp(branch_op, "BGE"))
                        opcode_value = 0x12;
                    else if (!strcmp(branch_op, "BLT"))
                        opcode_value = 0x13;
                    else if (!strcmp(branch_op, "BGT"))
                        opcode_value = 0x14;
                    else if (!strcmp(branch_op, "BLE"))
                        opcode_value = 0x15;
                    else if (!strcmp(branch_op, "BAL"))
                        opcode_value = 0x1E;

                    emit(out,
                         opcode_value,
                         0,
                         0,
                         (unsigned char)offset);

                    pc++;
                    continue;
                }
            }
        }

        /* ====================================================
         * ASSIGNMENT
         *
         * Everything below has the form:
         *
         * destination = expression
         * ==================================================== */

        char lhs[64];
        char rhs[160];

        if (sscanf(s, " %63[^=] = %159[^\n]",
                   lhs, rhs) != 2) {

            compiler_error(line_number + 1,
                           "invalid statement");
            fclose(out);
            exit(EXIT_FAILURE);
        }

        char *L = trim(lhs);
        char *R = trim(rhs);

        /* ====================================================
         * VECTOR STORE
         *
         * [xN] = vN
         *
         * 26 address-register vector-register 00
         * ==================================================== */

        if (L[0] == '[' && R[0] == 'v') {

            char address[64];

            if (sscanf(L, "[%63[^]]]", address) != 1 ||
                !is_valid_register(address) ||
                !is_valid_vector_register(R)) {

                compiler_error(line_number + 1,
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

        /* ====================================================
         * INTEGER STORE
         *
         * [xN] = xN
         *
         * 06 address-register source-register 00
         * ==================================================== */

        if (L[0] == '[') {

            char address[64];

            if (sscanf(L, "[%63[^]]]", address) != 1 ||
                !is_valid_register(address) ||
                !is_valid_register(R)) {

                compiler_error(line_number + 1,
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

        /* ====================================================
         * VECTOR DESTINATION
         * ==================================================== */

        if (L[0] == 'v') {

            int destination = vregnum(L);

            if (destination < 0) {
                compiler_error(line_number + 1,
                               "invalid vector destination");
                fclose(out);
                exit(EXIT_FAILURE);
            }

            /* ------------------------------------------------
             * Vector load
             *
             * v1 = [x2]
             * ------------------------------------------------ */

            if (R[0] == '[') {

                char address[64];

                if (sscanf(R, "[%63[^]]]", address) != 1) {
                    compiler_error(line_number + 1,
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

                if (parse_constant(address, &immediate)) {

                    /*
                     * The language permits constants as memory
                     * addresses. Use x255 as a temporary address
                     * register.
                     */
                    emit(out,
                         0x0F,
                         255,
                         0,
                         immediate);

                    emit(out,
                         0x25,
                         destination,
                         255,
                         0);

                    pc += 2;
                    continue;
                }

                compiler_error(line_number + 1,
                               "invalid vector memory address");
                fclose(out);
                exit(EXIT_FAILURE);
            }

            /* ------------------------------------------------
             * Vector arithmetic
             *
             * vD = vA + vB
             * vD = vA - vB
             * vD = vA * vB
             *
             * vD = vA + constant
             * vD = vA - constant
             * vD = vA * constant
             * ------------------------------------------------ */

            char a[64];
            char operation[8];
            char b[64];

            if (sscanf(R, "%63s %7s %63s",
                       a, operation, b) != 3) {

                compiler_error(line_number + 1,
                               "invalid vector expression");
                fclose(out);
                exit(EXIT_FAILURE);
            }

            if (strlen(operation) != 1 ||
                (operation[0] != '+' &&
                 operation[0] != '-' &&
                 operation[0] != '*')) {

                compiler_error(line_number + 1,
                               "invalid vector operator");
                fclose(out);
                exit(EXIT_FAILURE);
            }

            int va = vregnum(a);

            if (va < 0) {
                compiler_error(line_number + 1,
                               "first vector operand must be a vector register");
                fclose(out);
                exit(EXIT_FAILURE);
            }

            /* Vector + vector */

            if (is_valid_vector_register(b)) {

                int vb = vregnum(b);
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

            /* Vector + constant */

            int immediate;

            if (parse_constant(b, &immediate)) {

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
             * Vector operation with an integer register.
             *
             * The language specification explicitly permits an
             * integer register as the second operand, e.g.
             *
             *     v3 = v1 * x4
             *
             * The supplied opcode table does not provide a separate
             * opcode for vector-register/scalar-register arithmetic.
             *
             * Therefore this is handled by the processor implementation
             * using the vector arithmetic opcode convention.
             *
             * For now we encode the integer register number in src2.
             */
            if (is_valid_register(b)) {

                int scalar = regnum(b);

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

            compiler_error(line_number + 1,
                           "invalid second vector operand");
            fclose(out);
            exit(EXIT_FAILURE);
        }

        /* ====================================================
         * INTEGER DESTINATION
         * ==================================================== */

        int destination = regnum(L);

        if (destination < 0) {
            compiler_error(line_number + 1,
                           "invalid integer destination");
            fclose(out);
            exit(EXIT_FAILURE);
        }

        /* ----------------------------------------------------
         * Integer memory read
         *
         * x1 = [x2]
         * x1 = [32]
         * ---------------------------------------------------- */

        if (R[0] == '[') {

            char address[64];

            if (sscanf(R, "[%63[^]]]", address) != 1) {
                compiler_error(line_number + 1,
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

            if (parse_constant(address, &immediate)) {

                /*
                 * Materialise constant address into x255.
                 */
                emit(out,
                     0x0F,
                     255,
                     0,
                     immediate);

                emit(out,
                     0x05,
                     destination,
                     255,
                     0);

                pc += 2;
                continue;
            }

            compiler_error(line_number + 1,
                           "invalid memory address");
            fclose(out);
            exit(EXIT_FAILURE);
        }

        /* ----------------------------------------------------
         * Immediate data movement
         *
         * x1 = 25
         * ---------------------------------------------------- */

        {
            int immediate;

            if (parse_constant(R, &immediate)) {

                emit(out,
                     0x0F,
                     destination,
                     0,
                     immediate);

                pc++;
                continue;
            }
        }

        /* ----------------------------------------------------
         * Integer arithmetic
         *
         * x1 = x2 + x3
         * x1 = x2 + 10
         * ---------------------------------------------------- */

        {
            char a[64];
            char operation[8];
            char b[64];

            if (sscanf(R, "%63s %7s %63s",
                       a, operation, b) != 3) {

                compiler_error(line_number + 1,
                               "invalid integer expression");
                fclose(out);
                exit(EXIT_FAILURE);
            }

            if (strlen(operation) != 1 ||
                (operation[0] != '+' &&
                 operation[0] != '-' &&
                 operation[0] != '*' &&
                 operation[0] != '/')) {

                compiler_error(line_number + 1,
                               "invalid integer operator");
                fclose(out);
                exit(EXIT_FAILURE);
            }

            int operand1 = regnum(a);

            if (operand1 < 0) {
                compiler_error(line_number + 1,
                               "first integer operand must be a register");
                fclose(out);
                exit(EXIT_FAILURE);
            }

            /* Arithmetic with a constant */

            int immediate;

            if (parse_constant(b, &immediate)) {

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

            /* Arithmetic with another register */

            int operand2 = regnum(b);

            if (operand2 < 0) {
                compiler_error(line_number + 1,
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
     * Opcode 00 terminates processor execution.
     *
     * This is essential: every normally compiled program has a
     * definite processor termination instruction.
     */
    emit(out, 0x00, 0, 0, 0);

    fclose(out);
}
