#define _DEFAULT_SOURCE
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "processor.h"
#include "memory.h"

uint32_t RegisterFile[NP][INT_REGS];
uint32_t VectorRegisterFile[NP][VEC_REGS][VEC_LANES];
uint32_t PC[NP];
uint8_t opcode[NP], dest[NP], src1[NP], src2[NP];
int N[NP], Z[NP], C[NP], V[NP];
int end_of_simulation[NP];

static uint32_t read32(int p, uint32_t addr) {
    uint32_t value = 0;

    for (int i = 0; i < 4; i++) {
        int physical = getPhysicallAddress(p, 0, (int)addr + i);
        if (physical < 0)
            return 0;
        value |= (uint32_t)memory[p][physical] << (8 * i);
    }

    return value;
}

static void write32(int p, uint32_t addr, uint32_t val) {
    for (int i = 0; i < 4; i++) {
        int physical = getPhysicallAddress(p, 0, (int)addr + i);
        if (physical < 0)
            return;
        memory[p][physical] = (uint8_t)((val >> (8 * i)) & 0xFF);
    }
}

static void update_flags_add(int p, uint32_t a, uint32_t b, uint32_t r) {
    Z[p] = (r == 0);
    N[p] = (r >> 31) & 1;
    C[p] = (r < a) || (r < b);

    int32_t sa = (int32_t)a;
    int32_t sb = (int32_t)b;
    int32_t sr = (int32_t)r;
    V[p] = ((sa >= 0 && sb >= 0 && sr < 0) ||
            (sa < 0 && sb < 0 && sr >= 0));
}

static void update_flags_sub(int p, uint32_t a, uint32_t b, uint32_t r) {
    Z[p] = (r == 0);
    N[p] = (r >> 31) & 1;
    C[p] = (a >= b);

    int32_t sa = (int32_t)a;
    int32_t sb = (int32_t)b;
    int32_t sr = (int32_t)r;
    V[p] = ((sa >= 0 && sb < 0 && sr < 0) ||
            (sa < 0 && sb >= 0 && sr >= 0));
}

void reset(int p) {
    if (p < 0 || p >= NP) return;
    memset(RegisterFile[p], 0, sizeof(RegisterFile[p]));
    memset(VectorRegisterFile[p], 0, sizeof(VectorRegisterFile[p]));
    PC[p] = 0;
    opcode[p] = dest[p] = src1[p] = src2[p] = 0;
    N[p] = Z[p] = C[p] = V[p] = 0;
    end_of_simulation[p] = 0;
}

void fetch(int p) {
    uint8_t *fields[4] = {&opcode[p], &dest[p], &src1[p], &src2[p]};

    for (int i = 0; i < 4; i++) {
        int physical = getPhysicallAddress(p, 1, (int)PC[p] + i);
        if (physical < 0) {
            opcode[p] = 0x00;
            dest[p] = src1[p] = src2[p] = 0;
            return;
        }
        *fields[i] = memory[p][physical];
    }
}

void decode(int p) {
    (void)p;
}

static int branch_taken(int p, uint8_t op) {
    switch (op) {
        case 0x10: return Z[p];
        case 0x11: return !Z[p];
        case 0x12: return N[p] == V[p];
        case 0x13: return N[p] != V[p];
        case 0x14: return (!Z[p]) && (N[p] == V[p]);
        case 0x15: return Z[p] || (N[p] != V[p]);
        case 0x1E: return 1;
        default: return 0;
    }
}

void execute(int p) {
    uint32_t a, b, r;

    switch (opcode[p]) {
        case 0x00:
            end_of_simulation[p] = 1;
            return;

        case 0x07:
        case 0x0F:
            RegisterFile[p][dest[p]] = src2[p];
            PC[p] += 4;
            break;

        case 0x01:
            a = RegisterFile[p][src1[p]];
            b = RegisterFile[p][src2[p]];
            r = a + b;
            RegisterFile[p][dest[p]] = r;
            update_flags_add(p, a, b, r);
            PC[p] += 4;
            break;

        case 0x09:
            a = RegisterFile[p][src1[p]];
            b = src2[p];
            r = a + b;
            RegisterFile[p][dest[p]] = r;
            update_flags_add(p, a, b, r);
            PC[p] += 4;
            break;

        case 0x02:
            a = RegisterFile[p][src1[p]];
            b = RegisterFile[p][src2[p]];
            r = a - b;
            RegisterFile[p][dest[p]] = r;
            update_flags_sub(p, a, b, r);
            PC[p] += 4;
            break;

        case 0x0A:
            a = RegisterFile[p][src1[p]];
            b = src2[p];
            r = a - b;
            RegisterFile[p][dest[p]] = r;
            update_flags_sub(p, a, b, r);
            PC[p] += 4;
            break;

        case 0x03:
            RegisterFile[p][dest[p]] =
                RegisterFile[p][src1[p]] * RegisterFile[p][src2[p]];
            PC[p] += 4;
            break;

        case 0x0B:
            RegisterFile[p][dest[p]] =
                RegisterFile[p][src1[p]] * src2[p];
            PC[p] += 4;
            break;

        case 0x04:
            if (RegisterFile[p][src2[p]] != 0)
                RegisterFile[p][dest[p]] =
                    RegisterFile[p][src1[p]] / RegisterFile[p][src2[p]];
            PC[p] += 4;
            break;

        case 0x0C:
            if (src2[p] != 0)
                RegisterFile[p][dest[p]] =
                    RegisterFile[p][src1[p]] / src2[p];
            PC[p] += 4;
            break;

        case 0x05:
            RegisterFile[p][dest[p]] = read32(p, RegisterFile[p][src1[p]]);
            PC[p] += 4;
            break;

        case 0x06:
            write32(p, RegisterFile[p][dest[p]], RegisterFile[p][src1[p]]);
            PC[p] += 4;
            break;

        case 0x08: {
            FILE *fd = fopen("processor.log", "a");
            if (fd) {
                fprintf(fd, "Process %d: x%u : %08X\n",
                        p, src2[p], RegisterFile[p][src2[p]]);
                fclose(fd);
            }
            PC[p] += 4;
            break;
        }

        case 0x21:
            for (int i = 0; i < 8; i++)
                VectorRegisterFile[p][dest[p]][i] =
                    VectorRegisterFile[p][src1[p]][i] +
                    VectorRegisterFile[p][src2[p]][i];
            PC[p] += 4;
            break;

        case 0x29:
            for (int i = 0; i < 8; i++)
                VectorRegisterFile[p][dest[p]][i] =
                    VectorRegisterFile[p][src1[p]][i] + src2[p];
            PC[p] += 4;
            break;

        case 0x22:
            for (int i = 0; i < 8; i++)
                VectorRegisterFile[p][dest[p]][i] =
                    VectorRegisterFile[p][src1[p]][i] -
                    VectorRegisterFile[p][src2[p]][i];
            PC[p] += 4;
            break;

        case 0x2A:
            for (int i = 0; i < 8; i++)
                VectorRegisterFile[p][dest[p]][i] =
                    VectorRegisterFile[p][src1[p]][i] - src2[p];
            PC[p] += 4;
            break;

        case 0x23:
            for (int i = 0; i < 8; i++)
                VectorRegisterFile[p][dest[p]][i] =
                    VectorRegisterFile[p][src1[p]][i] *
                    VectorRegisterFile[p][src2[p]][i];
            PC[p] += 4;
            break;

        case 0x2B:
            for (int i = 0; i < 8; i++)
                VectorRegisterFile[p][dest[p]][i] =
                    VectorRegisterFile[p][src1[p]][i] * src2[p];
            PC[p] += 4;
            break;

        case 0x25: {
            uint32_t addr = RegisterFile[p][src1[p]];
            for (int i = 0; i < 8; i++)
                VectorRegisterFile[p][dest[p]][i] = read32(p, addr + i * 4);
            PC[p] += 4;
            break;
        }

        case 0x26: {
            uint32_t addr = RegisterFile[p][dest[p]];
            for (int i = 0; i < 8; i++)
                write32(p, addr + i * 4, VectorRegisterFile[p][src1[p]][i]);
            PC[p] += 4;
            break;
        }

        default:
            if (branch_taken(p, opcode[p])) {
                int8_t off = (int8_t)src2[p];
                PC[p] = PC[p] + (int32_t)off * 4;
            } else {
                PC[p] += 4;
            }
            break;
    }
}

void process_instructions(int p, int instruction_count) {
    for (int i = 0; i < instruction_count && !end_of_simulation[p]; i++) {
        fetch(p);
        decode(p);
        execute(p);
    }
    usleep(10);
}
