#include <stdio.h>
#include "compiler.h"
#include "memory.h"
#include "processor.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <program.txt>\n", argv[0]);
        return 1;
    }

    compile(argv[1]);

    initialize();
    reset();

    while (!end_of_simulation) {
        fetch();
        decode();
        execute();
    }

    finalize();

    return 0;
}
