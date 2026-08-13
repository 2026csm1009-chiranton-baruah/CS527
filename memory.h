#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

extern uint8_t Instruction[256];
extern uint8_t Data[4096];

void initialize();
void finalize();

#endif
