#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <bus.h>


typedef struct {
  uint16_t pc; // Program Counter
  uint16_t reg[16]; // Registers
  int halted; // Halt flag
  uint64_t cycles; // Instructions executed since reset
} CPU;

CPU* cpu_init();
void cpu_reset(CPU *cpu);

void cpu_step(CPU *cpu, BUS *bus);


#endif
