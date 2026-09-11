#include "disk.h"

#include <stdio.h>
#include <string.h>

#define DISK_MAGIC "SIMDISK1"
#define DISK_MAGIC_SIZE 8

static FILE *disk_fp = NULL;
static uint8_t block_bitmap[NUM_DISK_BLOCKS];

static int valid_block(int block)
{
    return block >= 0 && block < NUM_DISK_BLOCKS;
}

static int read_metadata(void)
{
    uint8_t metadata[DISK_BLOCK_SIZE];
    memset(metadata, 0, sizeof(metadata));

    if (fseek(disk_fp, 0, SEEK_SET) != 0)
        return DISK_ERROR;

    if (fread(metadata, 1, sizeof(metadata), disk_fp) != sizeof(metadata))
        return DISK_ERROR;

    if (memcmp(metadata, DISK_MAGIC, DISK_MAGIC_SIZE) != 0)
        return 1; /* New/unformatted disk. */

    memcpy(block_bitmap,
           metadata + DISK_MAGIC_SIZE,
           NUM_DISK_BLOCKS);

    block_bitmap[0] = 1;
    return DISK_OK;
}

static int write_metadata(void)
{
    uint8_t metadata[DISK_BLOCK_SIZE];
    memset(metadata, 0, sizeof(metadata));

    memcpy(metadata, DISK_MAGIC, DISK_MAGIC_SIZE);
    memcpy(metadata + DISK_MAGIC_SIZE,
           block_bitmap,
           NUM_DISK_BLOCKS);

    if (fseek(disk_fp, 0, SEEK_SET) != 0)
        return DISK_ERROR;

    if (fwrite(metadata, 1, sizeof(metadata), disk_fp) != sizeof(metadata))
        return DISK_ERROR;

    return fflush(disk_fp) == 0 ? DISK_OK : DISK_ERROR;
}

int disk_initialize(const char *disk_file)
{
    if (disk_file == NULL)
        return DISK_INVALID;

    disk_fp = fopen(disk_file, "r+b");
    if (disk_fp == NULL)
        disk_fp = fopen(disk_file, "w+b");

    if (disk_fp == NULL)
        return DISK_ERROR;

    if (fseek(disk_fp, DISK_SIZE - 1, SEEK_SET) != 0 ||
        fputc(0, disk_fp) == EOF ||
        fflush(disk_fp) != 0) {
        fclose(disk_fp);
        disk_fp = NULL;
        return DISK_ERROR;
    }

    memset(block_bitmap, 0, sizeof(block_bitmap));

    int result = read_metadata();
    if (result == DISK_OK) {
        return DISK_OK;
    }

    if (result == 1) {
        block_bitmap[0] = 1;
        return write_metadata();
    }

    fclose(disk_fp);
    disk_fp = NULL;
    return DISK_ERROR;
}

void disk_finalize(void)
{
    if (disk_fp == NULL)
        return;

    write_metadata();
    fclose(disk_fp);
    disk_fp = NULL;
    memset(block_bitmap, 0, sizeof(block_bitmap));
}

int disk_read_block(int block, uint8_t *buffer)
{
    if (disk_fp == NULL || buffer == NULL || !valid_block(block))
        return DISK_INVALID;

    if (fseek(disk_fp, (long)block * DISK_BLOCK_SIZE, SEEK_SET) != 0)
        return DISK_ERROR;

    return fread(buffer, 1, DISK_BLOCK_SIZE, disk_fp) == DISK_BLOCK_SIZE
               ? DISK_OK : DISK_ERROR;
}

int disk_write_block(int block, const uint8_t *buffer)
{
    if (disk_fp == NULL || buffer == NULL || !valid_block(block))
        return DISK_INVALID;

    if (fseek(disk_fp, (long)block * DISK_BLOCK_SIZE, SEEK_SET) != 0)
        return DISK_ERROR;

    if (fwrite(buffer, 1, DISK_BLOCK_SIZE, disk_fp) != DISK_BLOCK_SIZE)
        return DISK_ERROR;

    return fflush(disk_fp) == 0 ? DISK_OK : DISK_ERROR;
}

int disk_allocate_block(void)
{
    if (disk_fp == NULL)
        return DISK_ERROR;

    for (int block = 1; block < NUM_DISK_BLOCKS; block++) {
        if (block_bitmap[block] == 0) {
            block_bitmap[block] = 1;
            if (write_metadata() != DISK_OK) {
                block_bitmap[block] = 0;
                return DISK_ERROR;
            }
            return block;
        }
    }

    return DISK_FULL;
}

int disk_free_block(int block)
{
    if (disk_fp == NULL)
        return DISK_ERROR;

    if (block <= 0 || block >= NUM_DISK_BLOCKS)
        return DISK_INVALID;

    block_bitmap[block] = 0;
    return write_metadata();
}

int disk_is_block_free(int block)
{
    if (!valid_block(block))
        return DISK_INVALID;

    return block_bitmap[block] == 0;
}

void disk_print_status(void)
{
    if (disk_fp == NULL) {
        printf("[DISK] not initialized\n");
        return;
    }

    int used = 0;
    for (int i = 0; i < NUM_DISK_BLOCKS; i++)
        used += block_bitmap[i] != 0;

    printf("[DISK] %d blocks total, %d used, %d free\n",
           NUM_DISK_BLOCKS, used, NUM_DISK_BLOCKS - used);
}
