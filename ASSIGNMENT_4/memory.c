#include <stdio.h>
#include <stdint.h>
#include "memory.h"

uint8_t Instruction[NP][256];
uint8_t Data[NP][4096];

static void clear_memory(int proc_id) {
    for (int i = 0; i < 256; i++)
        Instruction[proc_id][i] = 0;
    for (int i = 0; i < 4096; i++)
        Data[proc_id][i] = 0;
}

static void load_bytes(const char *fname, uint8_t *mem, int max) {
    FILE *fp = fopen(fname, "r");
    if (!fp) return;

    unsigned int b;
    int i = 0;
    while (fscanf(fp, "%x", &b) == 1 && i < max)
        mem[i++] = (uint8_t)b;

    fclose(fp);
}

static void save_bytes(const char *fname, uint8_t *mem, int size) {
    FILE *fp = fopen(fname, "w");
    if (!fp) return;

    for (int i = 0; i < size; i += 4) {
        fprintf(fp, "%02X %02X %02X %02X\n",
                mem[i], mem[i + 1], mem[i + 2], mem[i + 3]);
    }
    fclose(fp);
}

void initialize_memory(int proc_id, const char *program_file, const char *data_file) {
    if (proc_id < 0 || proc_id >= NP) return;

    clear_memory(proc_id);
    load_bytes(program_file, Instruction[proc_id], 256);
    load_bytes(data_file, Data[proc_id], 4096);
}

void finalize_memory(int proc_id, const char *data_file) {
    if (proc_id < 0 || proc_id >= NP) return;
    save_bytes(data_file, Data[proc_id], 4096);
}

/* Compatibility helpers for the old single-process interface. */
void initialize(void) {
    for (int p = 0; p < NP; p++)
        clear_memory(p);
    load_bytes("program.byte", Instruction[0], 256);
    load_bytes("data.byte", Data[0], 4096);
}

void finalize(void) {
    save_bytes("data.byte", Data[0], 4096);
}
