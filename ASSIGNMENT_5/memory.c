#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memory.h"

uint8_t memory[NP][MEMSIZE];
int pageTable[NP][NUM_LOGICAL_PAGES];

/* Frame 0 is reserved by the Lab 5 specification. */
static uint8_t freePages[NP][NUM_PHYSICAL_PAGES];
static int memory_initialized = 0;

static void reset_page_table(int proc_id) {
    for (int i = 0; i < NUM_LOGICAL_PAGES; i++)
        pageTable[proc_id][i] = -1;
}

void memory_system_init(void) {
    memset(memory, 0, sizeof(memory));
    memset(freePages, 0, sizeof(freePages));

    for (int p = 0; p < NP; p++) {
        /* 0 = free, 1 = allocated. Frame 0 is permanently reserved. */
        freePages[p][0] = 1;
        reset_page_table(p);
    }
    memory_initialized = 1;
}

int getFreePage(int proc_id) {
    if (proc_id < 0 || proc_id >= NP)
        return -1;

    for (int frame = 1; frame < NUM_PHYSICAL_PAGES; frame++) {
        if (!freePages[proc_id][frame]) {
            freePages[proc_id][frame] = 1;
            return frame;
        }
    }

    fprintf(stderr, "MMU: no free physical frame available\n");
    return -1;
}

static void free_process_frames(int proc_id) {
    if (proc_id < 0 || proc_id >= NP)
        return;

    for (int i = 0; i < NUM_LOGICAL_PAGES; i++) {
        int frame = pageTable[proc_id][i];
        if (frame > 0 && frame < NUM_PHYSICAL_PAGES)
            freePages[proc_id][frame] = 0;
        pageTable[proc_id][i] = -1;
    }
}

/*
 * Translate a logical address to a physical address.
 *
 * Instruction logical space:  0 .. INSTR_MEMSIZE-1
 * Data logical space:         0 .. DATA_MEMSIZE-1
 * Data starts at logical page INSTR_MEMSIZE/PAGESIZE.
 */
int getPhysicallAddress(int proc_id, int isFetch, int address) {
    if (proc_id < 0 || proc_id >= NP || address < 0)
        return -1;

    int logical_page;
    int offset;

    if (isFetch) {
        if (address >= INSTR_MEMSIZE)
            return -1;
        logical_page = address / PAGESIZE;
        offset = address % PAGESIZE;
    } else {
        if (address >= DATA_MEMSIZE)
            return -1;
        logical_page = address / PAGESIZE + INSTR_MEMSIZE / PAGESIZE;
        offset = address % PAGESIZE;
    }

    if (logical_page < 0 || logical_page >= NUM_LOGICAL_PAGES)
        return -1;

    int frame = pageTable[proc_id][logical_page];
    if (frame < 0)
        return -1;

    return frame * PAGESIZE + offset;
}

static int load_hex_bytes(const char *fname, uint8_t *dst, int max) {
    FILE *fp = fopen(fname, "r");
    if (!fp)
        return -1;

    unsigned int b;
    int i = 0;
    while (i < max && fscanf(fp, "%x", &b) == 1)
        dst[i++] = (uint8_t)b;

    fclose(fp);
    return i;
}

static int allocate_pages_for_range(int proc_id, int first_page,
                                    int byte_count, const uint8_t *src) {
    int pages = (byte_count + PAGESIZE - 1) / PAGESIZE;

    for (int i = 0; i < pages; i++) {
        int logical_page = first_page + i;
        if (logical_page >= NUM_LOGICAL_PAGES)
            return -1;

        int frame = getFreePage(proc_id);
        if (frame < 0)
            return -1;

        pageTable[proc_id][logical_page] = frame;

        int bytes_this_page = byte_count - i * PAGESIZE;
        if (bytes_this_page > PAGESIZE)
            bytes_this_page = PAGESIZE;
        if (bytes_this_page > 0)
            memcpy(&memory[proc_id][frame * PAGESIZE],
                   src + i * PAGESIZE,
                   (size_t)bytes_this_page);
    }

    return 0;
}

void initialize_memory(int proc_id, const char *program_file, const char *data_file) {
    if (proc_id < 0 || proc_id >= NP)
        return;
    if (!memory_initialized)
        memory_system_init();

    /* A fresh process must start with no mappings. */
    free_process_frames(proc_id);

    uint8_t program[INSTR_MEMSIZE];
    uint8_t data[DATA_MEMSIZE];
    memset(program, 0, sizeof(program));
    memset(data, 0, sizeof(data));

    int program_bytes = load_hex_bytes(program_file, program, INSTR_MEMSIZE);
    int data_bytes = load_hex_bytes(data_file, data, DATA_MEMSIZE);

    if (program_bytes < 0) {
        fprintf(stderr, "MMU: cannot open %s\n", program_file);
        program_bytes = 0;
    }
    if (data_bytes < 0) {
        fprintf(stderr, "MMU: cannot open %s\n", data_file);
        data_bytes = 0;
    }

    /* Instruction pages begin at logical page 0. */
    if (program_bytes > 0 &&
        allocate_pages_for_range(proc_id, 0, program_bytes, program) < 0) {
        fprintf(stderr, "MMU: insufficient memory for process %d program\n", proc_id);
        free_process_frames(proc_id);
        return;
    }

    /* Data pages begin after the instruction logical address space. */
    if (allocate_pages_for_range(proc_id,
                                 INSTR_MEMSIZE / PAGESIZE,
                                 DATA_MEMSIZE, data) < 0) {
        fprintf(stderr, "MMU: insufficient memory for process %d data\n", proc_id);
        free_process_frames(proc_id);
    }
}

void finalize_memory(int proc_id, const char *data_file) {
    if (proc_id < 0 || proc_id >= NP)
        return;

    uint8_t data[DATA_MEMSIZE];
    memset(data, 0, sizeof(data));

    /* Reconstruct logical data memory from its physical frames. */
    for (int i = 0; i < DATA_MEMSIZE; i++) {
        int physical = getPhysicallAddress(proc_id, 0, i);
        if (physical >= 0)
            data[i] = memory[proc_id][physical];
    }

    FILE *fp = fopen(data_file, "w");
    if (fp) {
        for (int i = 0; i < DATA_MEMSIZE; i += 4) {
            fprintf(fp, "%02X %02X %02X %02X\n",
                    data[i], data[i + 1], data[i + 2], data[i + 3]);
        }
        fclose(fp);
    }

    free_process_frames(proc_id);
}

void initialize(void) {
    memory_system_init();
    initialize_memory(0, "program.byte", "data.byte");
}

void finalize(void) {
    finalize_memory(0, "data.byte");
}
