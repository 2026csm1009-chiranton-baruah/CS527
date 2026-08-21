#include <stdint.h>
#include <string.h>
#include "processor.h"
#include "memory.h"

/* Integer registers */
uint32_t RegisterFile[256];

/* Vector registers: 32 registers, each 8 x 32-bit lanes */
uint32_t VectorRegisterFile[32][8];

uint32_t PC;
uint8_t opcode, dest, src1, src2;

int N, Z, C, V;
int end_of_simulation;

/* -------------------------------------------------- */
/* Memory helpers                                     */
/* -------------------------------------------------- */

static uint32_t read32(uint32_t addr) {
    if (addr + 3 >= 4096) return 0;

    return (uint32_t)Data[addr] |
           ((uint32_t)Data[addr + 1] << 8) |
           ((uint32_t)Data[addr + 2] << 16) |
           ((uint32_t)Data[addr + 3] << 24);
}

static void write32(uint32_t addr, uint32_t val) {
    if (addr + 3 >= 4096) return;

    Data[addr]     = val & 0xFF;
    Data[addr + 1] = (val >> 8) & 0xFF;
    Data[addr + 2] = (val >> 16) & 0xFF;
    Data[addr + 3] = (val >> 24) & 0xFF;
}

/* -------------------------------------------------- */
/* Flag helpers                                       */
/* -------------------------------------------------- */

static void update_flags_add(uint32_t a, uint32_t b, uint32_t r) {
    Z = (r == 0);
    N = (r >> 31) & 1;
    C = (r < a) || (r < b);

    int32_t sa = (int32_t)a;
    int32_t sb = (int32_t)b;
    int32_t sr = (int32_t)r;

    V = ((sa >= 0 && sb >= 0 && sr < 0) ||
         (sa < 0 && sb < 0 && sr >= 0));
}

static void update_flags_sub(uint32_t a, uint32_t b, uint32_t r) {
    Z = (r == 0);
    N = (r >> 31) & 1;
    C = (a >= b);

    int32_t sa = (int32_t)a;
    int32_t sb = (int32_t)b;
    int32_t sr = (int32_t)r;

    V = ((sa >= 0 && sb < 0 && sr < 0) ||
         (sa < 0 && sb >= 0 && sr >= 0));
}

/* -------------------------------------------------- */
/* Processor control                                  */
/* -------------------------------------------------- */

void reset() {
    memset(RegisterFile, 0, sizeof(RegisterFile));
    memset(VectorRegisterFile, 0, sizeof(VectorRegisterFile));

    PC = 0;
    opcode = dest = src1 = src2 = 0;

    N = Z = C = V = 0;

    end_of_simulation = 0;
}

void fetch() {
    opcode = Instruction[PC];
    dest   = Instruction[PC + 1];
    src1   = Instruction[PC + 2];
    src2   = Instruction[PC + 3];
}

void decode() {
    /* Empty by design */
}

/* -------------------------------------------------- */
/* Branch helper                                      */
/* -------------------------------------------------- */

static int branch_taken(uint8_t op) {
    switch (op) {
        case 0x10: return Z;			// EQ
        case 0x11: return !Z;			// NE
        case 0x12: return N == V;		// CS
        case 0x13: return N != V;		// CC
        case 0x14: return (!Z) && (N == V);	// MI
        case 0x15: return Z || (N != V);	// PL
        case 0x1E: return 1;			// AL
        default:   return 0;
    }
}

/* -------------------------------------------------- */
/* Execute                                            */
/* -------------------------------------------------- */

void execute() {
    uint32_t a, b, r;

    switch (opcode) {

        /* HALT */
        case 0x00:
            end_of_simulation = 1;
            return;

        /* MOV immediate */
        case 0x07:
        case 0x0F:
            RegisterFile[dest] = src2;
            PC += 4;
            break;

        /* Integer ADD */
        case 0x01:
            a = RegisterFile[src1];
            b = RegisterFile[src2];
            r = a + b;
            RegisterFile[dest] = r;
            update_flags_add(a, b, r);
            PC += 4;
            break;

        /* Integer ADD immediate */
        case 0x09:
            a = RegisterFile[src1];
            b = src2;
            r = a + b;
            RegisterFile[dest] = r;
            update_flags_add(a, b, r);
            PC += 4;
            break;

        /* Integer SUB */
        case 0x02:
            a = RegisterFile[src1];
            b = RegisterFile[src2];
            r = a - b;
            RegisterFile[dest] = r;
            update_flags_sub(a, b, r);
            PC += 4;
            break;

        /* Integer SUB immediate */
        case 0x0A:
            a = RegisterFile[src1];
            b = src2;
            r = a - b;
            RegisterFile[dest] = r;
            update_flags_sub(a, b, r);
            PC += 4;
            break;

        /* Integer MUL */
        case 0x03:
            RegisterFile[dest] =
                RegisterFile[src1] * RegisterFile[src2];
            PC += 4;
            break;

        /* Integer MUL immediate */
        case 0x0B:
            RegisterFile[dest] =
                RegisterFile[src1] * src2;
            PC += 4;
            break;

        /* Integer DIV */
        case 0x04:
            if (RegisterFile[src2] != 0)
                RegisterFile[dest] =
                    RegisterFile[src1] / RegisterFile[src2];
            PC += 4;
            break;

        /* Integer DIV immediate */
        case 0x0C:
            if (src2 != 0)
                RegisterFile[dest] =
                    RegisterFile[src1] / src2;
            PC += 4;
            break;

        /* Integer LOAD */
        case 0x05:
            RegisterFile[dest] = read32(RegisterFile[src1]);
            PC += 4;
            break;

        /* Integer STORE */
        case 0x06:
            write32(RegisterFile[dest], RegisterFile[src1]);
            PC += 4;
            break;

        /* -------------------------------------------------- */
        /* Vector ADD vv                                      */
        /* -------------------------------------------------- */
        case 0x21:
            for (int i = 0; i < 8; i++)
                VectorRegisterFile[dest][i] =
                    VectorRegisterFile[src1][i] +
                    VectorRegisterFile[src2][i];
            PC += 4;
            break;

        /* Vector ADD immediate */
        case 0x29:
            for (int i = 0; i < 8; i++)
                VectorRegisterFile[dest][i] =
                    VectorRegisterFile[src1][i] + src2;
            PC += 4;
            break;

        /* Vector SUB vv */
        case 0x22:
            for (int i = 0; i < 8; i++)
                VectorRegisterFile[dest][i] =
                    VectorRegisterFile[src1][i] -
                    VectorRegisterFile[src2][i];
            PC += 4;
            break;

        /* Vector SUB immediate */
        case 0x2A:
            for (int i = 0; i < 8; i++)
                VectorRegisterFile[dest][i] =
                    VectorRegisterFile[src1][i] - src2;
            PC += 4;
            break;

        /* Vector MUL vv */
        case 0x23:
            for (int i = 0; i < 8; i++)
                VectorRegisterFile[dest][i] =
                    VectorRegisterFile[src1][i] *
                    VectorRegisterFile[src2][i];
            PC += 4;
            break;

        /* Vector MUL immediate */
        case 0x2B:
            for (int i = 0; i < 8; i++)
                VectorRegisterFile[dest][i] =
                    VectorRegisterFile[src1][i] * src2;
            PC += 4;
            break;

        /* Vector LOAD */
        case 0x25: {
            uint32_t addr = RegisterFile[src1];

            for (int i = 0; i < 8; i++)
                VectorRegisterFile[dest][i] =
                    read32(addr + i * 4);

            PC += 4;
            break;
        }

        /* Vector STORE */
        case 0x26: {
            uint32_t addr = RegisterFile[dest];

            for (int i = 0; i < 8; i++)
                write32(addr + i * 4,
                        VectorRegisterFile[src1][i]);

            PC += 4;
            break;
        }

        /* Branches */
        default:
            if (branch_taken(opcode)) {
                int8_t off = (int8_t)src2;
                PC = PC + off * 4;
            } else {
                PC += 4;
            }
            break;
    }
}
