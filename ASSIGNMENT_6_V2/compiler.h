#ifndef COMPILER_H
#define COMPILER_H

/*
 * Compile a source program into program.byte.
 *
 * The compiler:
 *   - reads the source file,
 *   - resolves labels,
 *   - translates instructions into four-byte bytecode,
 *   - writes the resulting program to program.byte.
 *
 * Compilation errors terminate compilation rather than
 * producing potentially invalid bytecode.
 */
void compile(const char *filename);

#endif
