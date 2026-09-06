#include "disk_tests.h"
#include "disk.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

static int tests_run = 0;
static int tests_passed = 0;

#define CHECK(condition, message) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("[PASS] %s\n", message); \
    } else { \
        printf("[FAIL] %s\n", message); \
        return -1; \
    } \
} while (0)

static int test_basic_io(void)
{
    uint8_t write_buffer[DISK_BLOCK_SIZE];
    uint8_t read_buffer[DISK_BLOCK_SIZE];

    for (int i = 0; i < DISK_BLOCK_SIZE; ++i)
        write_buffer[i] = (uint8_t)(i & 0xFF);

    memset(read_buffer, 0, sizeof(read_buffer));

    int block = disk_allocate_block();

    CHECK(block > 0 && block < NUM_DISK_BLOCKS,
          "allocate a valid data block");

    CHECK(disk_write_block(block, write_buffer) == DISK_OK,
          "write one complete block");

    CHECK(disk_read_block(block, read_buffer) == DISK_OK,
          "read one complete block");

    CHECK(memcmp(write_buffer, read_buffer, DISK_BLOCK_SIZE) == 0,
          "read data matches written data");

    CHECK(disk_free_block(block) == DISK_OK,
          "free allocated block");

    CHECK(disk_is_block_free(block) == 1,
          "freed block is reported as free");

    return 0;
}

static int test_block_zero_reserved(void)
{
    CHECK(disk_is_block_free(0) == 0,
          "block 0 is reserved");

    CHECK(disk_free_block(0) == DISK_INVALID,
          "block 0 cannot be freed");

    uint8_t buffer[DISK_BLOCK_SIZE] = {0};

    CHECK(disk_read_block(0, buffer) == DISK_OK,
          "reserved block remains readable");

    return 0;
}

static int test_invalid_access(void)
{
    uint8_t buffer[DISK_BLOCK_SIZE] = {0};

    CHECK(disk_read_block(-1, buffer) == DISK_INVALID,
          "negative block number is rejected");

    CHECK(disk_read_block(NUM_DISK_BLOCKS, buffer) == DISK_INVALID,
          "block number beyond disk is rejected");

    CHECK(disk_write_block(-1, buffer) == DISK_INVALID,
          "negative block write is rejected");

    CHECK(disk_write_block(NUM_DISK_BLOCKS, buffer) == DISK_INVALID,
          "out-of-range block write is rejected");

    CHECK(disk_free_block(-1) == DISK_INVALID,
          "negative block free is rejected");

    CHECK(disk_free_block(NUM_DISK_BLOCKS) == DISK_INVALID,
          "out-of-range block free is rejected");

    return 0;
}

static int test_allocation(void)
{
    int blocks[NUM_DISK_BLOCKS];
    int count = 0;

    for (;;) {
        int block = disk_allocate_block();

        if (block == DISK_FULL)
            break;

        if (block < 1 || block >= NUM_DISK_BLOCKS) {
            printf("[FAIL] allocator returned invalid block %d\n", block);
            return -1;
        }

        blocks[count++] = block;

        if (count >= NUM_DISK_BLOCKS) {
            printf("[FAIL] allocator did not report full disk\n");
            return -1;
        }
    }

    CHECK(count == NUM_DISK_BLOCKS - 1,
          "allocator fills every usable block and leaves block 0 reserved");

    CHECK(disk_allocate_block() == DISK_FULL,
          "allocator reports DISK_FULL when no blocks remain");

    for (int i = 0; i < count; ++i) {
        CHECK(disk_free_block(blocks[i]) == DISK_OK,
              "free every allocated block");
    }

    CHECK(disk_allocate_block() > 0,
          "allocator works again after blocks are freed");

    return 0;
}

static int test_pattern_round_trip(void)
{
    uint8_t write_buffer[DISK_BLOCK_SIZE];
    uint8_t read_buffer[DISK_BLOCK_SIZE];

    int block = disk_allocate_block();

    CHECK(block > 0,
          "allocate block for pattern test");

    for (int i = 0; i < DISK_BLOCK_SIZE; ++i)
        write_buffer[i] = (uint8_t)((i * 37 + 13) & 0xFF);

    memset(read_buffer, 0, sizeof(read_buffer));

    CHECK(disk_write_block(block, write_buffer) == DISK_OK,
          "write non-trivial byte pattern");

    CHECK(disk_read_block(block, read_buffer) == DISK_OK,
          "read non-trivial byte pattern");

    CHECK(memcmp(write_buffer, read_buffer, DISK_BLOCK_SIZE) == 0,
          "non-trivial pattern survives round trip");

    CHECK(disk_free_block(block) == DISK_OK,
          "free pattern-test block");

    return 0;
}

int run_disk_tests(const char *disk_image)
{
    if (disk_image == NULL)
        return -1;

    tests_run = 0;
    tests_passed = 0;

    printf("============================================================\n");
    printf(" SIMULATED DISK TEST SUITE\n");
    printf("============================================================\n");
    printf("Disk size       : %d bytes\n", DISK_SIZE);
    printf("Block size      : %d bytes\n", DISK_BLOCK_SIZE);
    printf("Number of blocks: %d\n", NUM_DISK_BLOCKS);
    printf("Disk image      : %s\n\n", disk_image);

    remove(disk_image);

    CHECK(disk_initialize(disk_image) == DISK_OK,
          "initialize disk image");

    disk_print_status();

    printf("\n-- Basic block I/O --\n");
    if (test_basic_io() != 0)
        goto failure;

    printf("\n-- Reserved block --\n");
    if (test_block_zero_reserved() != 0)
        goto failure;

    printf("\n-- Invalid access --\n");
    if (test_invalid_access() != 0)
        goto failure;

    printf("\n-- Block allocation --\n");
    if (test_allocation() != 0)
        goto failure;

    printf("\n-- Data pattern --\n");
    if (test_pattern_round_trip() != 0)
        goto failure;

    disk_print_status();

    disk_finalize();

    printf("\n============================================================\n");
    printf(" RESULT: %d/%d checks passed\n", tests_passed, tests_run);
    printf("============================================================\n");

    remove(disk_image);

    return tests_passed == tests_run ? 0 : -1;

failure:
    disk_finalize();

    printf("\n============================================================\n");
    printf(" RESULT: FAILED (%d/%d checks passed)\n",
           tests_passed, tests_run);
    printf("============================================================\n");

    return -1;
}
