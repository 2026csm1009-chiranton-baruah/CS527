#ifndef MEMORY_H
#define MEMORY_H

extern unsigned char Instruction[256];
extern unsigned char Data[4096];

void initialize();
void finalize();

#endif
