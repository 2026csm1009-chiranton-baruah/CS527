#ifndef DISK_H
#define DISK_H

#include <stdint.h>

#define DISK_SIZE        65536
#define DISK_BLOCK_SIZE  512
#define NUM_DISK_BLOCKS  (DISK_SIZE / DISK_BLOCK_SIZE)

#define DISK_OK          0
#define DISK_ERROR      -1
#define DISK_INVALID    -2
#define DISK_FULL       -3

int disk_initialize(const char *disk_file);
void disk_finalize(void);
int disk_read_block(int block, uint8_t *buffer);
int disk_write_block(int block, const uint8_t *buffer);
int disk_allocate_block(void);
int disk_free_block(int block);
int disk_is_block_free(int block);
void disk_print_status(void);

#endif
