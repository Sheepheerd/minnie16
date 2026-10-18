#ifndef DEBUG_H
#define DEBUG_H

#include <stddef.h>
#include <stdint.h>

// Writes the assembly for one instruction word into buf, using the same syntax
// the assembler accepts. pc is the address of the word, used to resolve branch
// targets.
void disassemble(uint16_t raw, uint16_t pc, char *buf, size_t n);

#endif
