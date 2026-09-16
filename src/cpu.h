#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <bus.h>


typedef struct {
  uint16_t pc; // Program Counter
  uint16_t reg[15]; // Registers
  int halted; // Halt flag
} CPU;

CPU* cpu_init();

void cpu_step(CPU *cpu, BUS *bus, int debug);

#endif
