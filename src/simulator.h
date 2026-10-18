#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "bus.h"
#include "cpu.h"

#define MAX_BREAKPOINTS 32

typedef enum {
  SIM_OK,     // Instruction ran, keep going
  SIM_HALTED, // CPU halted
  SIM_BREAK,  // Stopped on a breakpoint (or ctrl+c)
} SIM_STATUS;

typedef struct {
  CPU *cpu;
  BUS *bus;
  const char *program; // Path the program was loaded from, used by reset
  uint16_t breakpoints[MAX_BREAKPOINTS];
  int num_breakpoints;
} SIMULATOR;

// Creates a simulator with the program at path loaded. Returns NULL on failure.
SIMULATOR *sim_init(const char *path);
void sim_free(SIMULATOR *sim);

// Resets the CPU and reloads the program. Breakpoints are kept.
int sim_reset(SIMULATOR *sim);

// Executes one instruction. Returns SIM_BREAK if the next instruction is on a
// breakpoint.
SIM_STATUS sim_step(SIMULATOR *sim);

// Executes until the CPU halts or reaches a breakpoint. The first instruction
// always runs, so continuing from a breakpoint makes progress.
SIM_STATUS sim_run(SIMULATOR *sim);

// Prints pc, the instruction count and all registers
void sim_print_regs(SIMULATOR *sim);

// Interactive step-through debugger. Returns when the user quits.
int debug(SIMULATOR *sim);

#endif
