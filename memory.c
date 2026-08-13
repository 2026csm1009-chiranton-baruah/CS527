#include <stdio.h>
#include <stdint.h>
#include "memory.h"

uint8_t Instruction[256];
uint8_t Data[4096];

static void load_bytes(const char *fname, uint8_t *mem, int max) {
    FILE *fp = fopen(fname, "r");
    if (!fp) return;

    unsigned int b;
    int i = 0;

    while (fscanf(fp, "%x", &b) == 1 && i < max) {
        mem[i++] = (uint8_t)b;
    }

    fclose(fp);
}

static void save_bytes(const char *fname, uint8_t *mem, int size) {
    FILE *fp = fopen(fname, "w");
    if (!fp) return;

    for (int i = 0; i < size; i += 4) {
        fprintf(fp, "%02X %02X %02X %02X\n",
                mem[i], mem[i+1], mem[i+2], mem[i+3]);
    }

    fclose(fp);
}

void initialize() {
    for (int i = 0; i < 256; i++) Instruction[i] = 0;
    for (int i = 0; i < 4096; i++) Data[i] = 0;

    load_bytes("program.byte", Instruction, 256);
    load_bytes("data.byte", Data, 4096);
}

void finalize() {
    save_bytes("data.byte", Data, 4096);
}
