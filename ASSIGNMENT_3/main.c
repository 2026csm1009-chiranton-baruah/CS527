#include <stdio.h>
#include "compiler.h"
#include "memory.h"
#include "processor.h"

int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Usage: %s <program.txt>\n", argv[0]);
        return 1;
    }

    /* Compile source program to program.byte */
    compile(argv[1]);

    /* Initialize instruction and data memory */
    initialize();

    /* Reset processor state */
    reset();

    /* Run simulation */
    while (!end_of_simulation) {
        fetch();
        decode();
        execute();
    }

    /* Write back data memory */
    finalize();

    return 0;
}
