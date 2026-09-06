#include "disk_tests.h"

#include <stdio.h>

int main(void)
{
    const char *disk_image = "disk_test.img";

    printf("Starting disk subsystem tests...\n\n");

    int result = run_disk_tests(disk_image);

    if (result == 0) {
        printf("\nAll disk tests passed.\n");
        return 0;
    }

    printf("\nDisk tests failed.\n");
    return 1;
}
