#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "processor.h"
#include "disk.h"

uint8_t memory[MEMSIZE];
int pageTable[NP][NUM_LOGICAL_PAGES];
int freePages[NUM_PHYSICAL_PAGES];

/* Disk block backing each logical page; -1 means no backing block. */
static int diskPageTable[NP][NUM_LOGICAL_PAGES];
static int pageDirty[NP][NUM_LOGICAL_PAGES];

/* Reverse mapping for resident physical frames. */
static int frameOwnerProc[NUM_PHYSICAL_PAGES];
static int frameOwnerPage[NUM_PHYSICAL_PAGES];
static unsigned long frameStamp[NUM_PHYSICAL_PAGES];
static unsigned long nextStamp;

static int valid_proc(int proc_id)
{
    return proc_id >= 0 && proc_id < NP;
}

static int valid_frame(int frame)
{
    return frame >= 0 && frame < NUM_PHYSICAL_PAGES;
}

static int valid_logical_page(int page)
{
    return page >= 0 && page < NUM_LOGICAL_PAGES;
}

static void clear_page_table(int proc_id)
{
    if (!valid_proc(proc_id))
        return;

    for (int page = 0; page < NUM_LOGICAL_PAGES; page++) {
        pageTable[proc_id][page] = -1;
        diskPageTable[proc_id][page] = -1;
        pageDirty[proc_id][page] = 0;
    }
}

static void initialize_physical_memory(void)
{
    memset(memory, 0, sizeof(memory));
    memset(frameOwnerProc, -1, sizeof(frameOwnerProc));
    memset(frameOwnerPage, -1, sizeof(frameOwnerPage));
    memset(frameStamp, 0, sizeof(frameStamp));
    nextStamp = 1;

    for (int frame = 0; frame < NUM_PHYSICAL_PAGES; frame++)
        freePages[frame] = 0;

    freePages[0] = 1;
    frameOwnerProc[0] = -1;
    frameOwnerPage[0] = -1;

    for (int proc = 0; proc < NP; proc++)
        clear_page_table(proc);
}

void initialize_memory(void)
{
    initialize_physical_memory();

    if (disk_initialize("disk.img") != DISK_OK) {
        fprintf(stderr, "ERROR: unable to initialize simulated disk\n");
    }
}

static int load_hex_file(const char *filename,
                         uint8_t *destination,
                         int max_bytes,
                         int *bytes_loaded)
{
    if (filename == NULL || destination == NULL || bytes_loaded == NULL || max_bytes < 0)
        return -1;

    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
        return -1;

    int count = 0;
    unsigned int value;

    while (count < max_bytes) {
        int result = fscanf(fp, "%x", &value);
        if (result != 1)
            break;

        if (value > 0xFF) {
            fclose(fp);
            return -1;
        }

        destination[count++] = (uint8_t)value;
    }

    if (!feof(fp)) {
        int c;
        do {
            c = fgetc(fp);
        } while (c == ' ' || c == '\t' || c == '\n' || c == '\r');

        if (c != EOF) {
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    *bytes_loaded = count;
    return 0;
}

static int pages_required(int bytes)
{
    if (bytes <= 0)
        return 0;

    return (bytes + PAGESIZE - 1) / PAGESIZE;
}

int getFreePage(void)
{
    for (int frame = 1; frame < NUM_PHYSICAL_PAGES; frame++) {
        if (freePages[frame] == 0) {
            freePages[frame] = 1;
            memset(&memory[frame * PAGESIZE], 0, PAGESIZE);
            frameOwnerProc[frame] = -1;
            frameOwnerPage[frame] = -1;
            frameStamp[frame] = nextStamp++;
            return frame;
        }
    }

    return -1;
}

void freePage(int frame)
{
    if (frame <= 0 || frame >= NUM_PHYSICAL_PAGES)
        return;

    if (freePages[frame] == 0)
        return;

    int owner_proc = frameOwnerProc[frame];
    int owner_page = frameOwnerPage[frame];

    if (valid_proc(owner_proc) && valid_logical_page(owner_page) &&
        pageTable[owner_proc][owner_page] == frame) {
        pageTable[owner_proc][owner_page] = -1;
    }

    freePages[frame] = 0;
    frameOwnerProc[frame] = -1;
    frameOwnerPage[frame] = -1;
    frameStamp[frame] = 0;
    memset(&memory[frame * PAGESIZE], 0, PAGESIZE);
}

static int choose_victim_frame(void)
{
    int victim = -1;
    unsigned long oldest = 0;

    for (int frame = 1; frame < NUM_PHYSICAL_PAGES; frame++) {
        if (freePages[frame] == 0)
            continue;

        if (frameOwnerProc[frame] < 0 || frameOwnerPage[frame] < 0)
            continue;

        if (victim < 0 || frameStamp[frame] < oldest) {
            victim = frame;
            oldest = frameStamp[frame];
        }
    }

    return victim;
}

static int allocate_page_frame(int proc_id, int logical_page)
{
    int frame = getFreePage();

    if (frame >= 0)
        return frame;

    frame = choose_victim_frame();
    if (frame < 0)
        return -1;

    int victim_proc = frameOwnerProc[frame];
    int victim_page = frameOwnerPage[frame];

    if (valid_proc(victim_proc) && valid_logical_page(victim_page)) {
        if (pageDirty[victim_proc][victim_page]) {
            int block = diskPageTable[victim_proc][victim_page];
            if (block < 0 ||
                disk_write_block(block, &memory[frame * PAGESIZE]) != DISK_OK) {
                return -1;
            }
        }

        pageTable[victim_proc][victim_page] = -1;
        pageDirty[victim_proc][victim_page] = 0;
    }

    memset(&memory[frame * PAGESIZE], 0, PAGESIZE);
    frameOwnerProc[frame] = proc_id;
    frameOwnerPage[frame] = logical_page;
    frameStamp[frame] = nextStamp++;
    return frame;
}

int page_in(int proc_id, int logical_page)
{
    if (!valid_proc(proc_id) || !valid_logical_page(logical_page))
        return -1;

    if (pageTable[proc_id][logical_page] > 0)
        return pageTable[proc_id][logical_page];

    int block = diskPageTable[proc_id][logical_page];
    if (block < 0)
        return -1;

    int frame = allocate_page_frame(proc_id, logical_page);
    if (frame < 0)
        return -1;

    if (disk_read_block(block, &memory[frame * PAGESIZE]) != DISK_OK) {
        freePage(frame);
        return -1;
    }

    pageTable[proc_id][logical_page] = frame;
    pageDirty[proc_id][logical_page] = 0;
    frameOwnerProc[frame] = proc_id;
    frameOwnerPage[frame] = logical_page;
    frameStamp[frame] = nextStamp++;

    return frame;
}

int page_out(int proc_id, int logical_page)
{
    if (!valid_proc(proc_id) || !valid_logical_page(logical_page))
        return -1;

    int frame = pageTable[proc_id][logical_page];
    if (frame <= 0)
        return 0;

    int block = diskPageTable[proc_id][logical_page];
    if (block < 0)
        return -1;

    if (pageDirty[proc_id][logical_page] &&
        disk_write_block(block, &memory[frame * PAGESIZE]) != DISK_OK)
        return -1;

    pageDirty[proc_id][logical_page] = 0;
    freePage(frame);
    return 0;
}

int getPhysicallAddress(int proc_id,
                        int isFetch,
                        int address)
{
    if (!valid_proc(proc_id) || address < 0)
        return -1;

    int logical_page;
    int offset;

    if (isFetch) {
        if (address >= INSTRUCTION_SIZE)
            return -1;
        logical_page = address / PAGESIZE;
        offset = address % PAGESIZE;
    } else {
        if (address >= DATA_SIZE)
            return -1;
        logical_page = address / PAGESIZE + INSTRUCTION_SIZE / PAGESIZE;
        offset = address % PAGESIZE;
    }

    if (!valid_logical_page(logical_page))
        return -1;

    int frame = page_in(proc_id, logical_page);
    if (frame <= 0)
        return -1;

    int physical_address = frame * PAGESIZE + offset;
    if (physical_address < 0 || physical_address >= MEMSIZE)
        return -1;

    return physical_address;
}

int read_physical_byte(int physical_address, uint8_t *value)
{
    if (value == NULL || physical_address < 0 || physical_address >= MEMSIZE)
        return -1;

    *value = memory[physical_address];
    return 0;
}

int write_physical_byte(int physical_address, uint8_t value)
{
    if (physical_address < 0 || physical_address >= MEMSIZE)
        return -1;

    memory[physical_address] = value;
    return 0;
}

int read_process_byte(int proc_id,
                      int isFetch,
                      int logical_address,
                      uint8_t *value)
{
    int physical = getPhysicallAddress(proc_id, isFetch, logical_address);
    if (physical < 0)
        return -1;

    return read_physical_byte(physical, value);
}

int write_process_byte(int proc_id,
                       int logical_address,
                       uint8_t value)
{
    int physical = getPhysicallAddress(proc_id, 0, logical_address);
    if (physical < 0)
        return -1;

    int page = logical_address / PAGESIZE + INSTRUCTION_SIZE / PAGESIZE;
    if (valid_logical_page(page))
        pageDirty[proc_id][page] = 1;

    return write_physical_byte(physical, value);
}

static int store_logical_page(int proc_id,
                              int logical_page,
                              const uint8_t *bytes)
{
    if (!valid_proc(proc_id) || !valid_logical_page(logical_page) || bytes == NULL)
        return -1;

    int block = disk_allocate_block();
    if (block < 0)
        return -1;

    if (disk_write_block(block, bytes) != DISK_OK) {
        disk_free_block(block);
        return -1;
    }

    diskPageTable[proc_id][logical_page] = block;
    pageTable[proc_id][logical_page] = -1;
    pageDirty[proc_id][logical_page] = 0;
    return 0;
}

int load_process_memory(int proc_id,
                        const char *program_file,
                        const char *data_file)
{
    if (!valid_proc(proc_id) || program_file == NULL)
        return -1;

    release_process_memory(proc_id);

    uint8_t instruction_bytes[INSTRUCTION_SIZE];
    uint8_t data_bytes[DATA_SIZE];
    memset(instruction_bytes, 0, sizeof(instruction_bytes));
    memset(data_bytes, 0, sizeof(data_bytes));

    int instruction_count = 0;
    int data_count = 0;

    if (load_hex_file(program_file,
                      instruction_bytes,
                      INSTRUCTION_SIZE,
                      &instruction_count) != 0) {
        fprintf(stderr, "ERROR: cannot load program file %s\n", program_file);
        return -1;
    }

    int instruction_pages = pages_required(instruction_count);
    if (instruction_pages == 0)
        instruction_pages = 1;

    for (int page = 0; page < instruction_pages; page++) {
        uint8_t page_buffer[PAGESIZE];
        memset(page_buffer, 0, sizeof(page_buffer));

        int start = page * PAGESIZE;
        int remaining = instruction_count - start;
        int count = remaining > PAGESIZE ? PAGESIZE : remaining;
        if (count > 0)
            memcpy(page_buffer, instruction_bytes + start, count);

        if (store_logical_page(proc_id, page, page_buffer) != 0) {
            release_process_memory(proc_id);
            return -1;
        }
    }

    if (data_file != NULL && data_file[0] != '\0') {
        if (load_hex_file(data_file,
                          data_bytes,
                          DATA_SIZE,
                          &data_count) != 0) {
            fprintf(stderr, "ERROR: cannot load data file %s\n", data_file);
            release_process_memory(proc_id);
            return -1;
        }
    }

    while (data_count > 0 && data_bytes[data_count - 1] == 0)
        data_count--;

    int data_pages = pages_required(data_count);
    int first_data_page = INSTRUCTION_SIZE / PAGESIZE;

    for (int page = 0; page < data_pages; page++) {
        uint8_t page_buffer[PAGESIZE];
        memset(page_buffer, 0, sizeof(page_buffer));

        int start = page * PAGESIZE;
        int remaining = data_count - start;
        int count = remaining > PAGESIZE ? PAGESIZE : remaining;
        if (count > 0)
            memcpy(page_buffer, data_bytes + start, count);

        if (store_logical_page(proc_id,
                                first_data_page + page,
                                page_buffer) != 0) {
            release_process_memory(proc_id);
            return -1;
        }
    }

    printf("[MEM] proc=%d: instruction=%d bytes (%d pages), "
           "data=%d bytes (%d pages) [disk-backed]\n",
           proc_id,
           instruction_count,
           instruction_pages,
           data_count,
           data_pages);

    return 0;
}

void unload_process_memory(int proc_id)
{
    if (valid_proc(proc_id))
        release_process_memory(proc_id);
}

void release_process_memory(int proc_id)
{
    if (!valid_proc(proc_id))
        return;

    for (int page = 0; page < NUM_LOGICAL_PAGES; page++) {
        int frame = pageTable[proc_id][page];
        int block = diskPageTable[proc_id][page];

        if (frame > 0 && valid_frame(frame)) {
            if (pageDirty[proc_id][page] && block >= 0)
                disk_write_block(block, &memory[frame * PAGESIZE]);

            freePage(frame);
        }

        if (block >= 0)
            disk_free_block(block);

        pageTable[proc_id][page] = -1;
        diskPageTable[proc_id][page] = -1;
        pageDirty[proc_id][page] = 0;
    }
}

void finalize_memory(void)
{
    for (int proc = 0; proc < NP; proc++)
        release_process_memory(proc);

    memset(memory, 0, sizeof(memory));

    for (int frame = 0; frame < NUM_PHYSICAL_PAGES; frame++)
        freePages[frame] = 0;

    freePages[0] = 1;
    disk_finalize();
}

void initialize(void)
{
    initialize_memory();
}

void finalize(void)
{
    finalize_memory();
}
