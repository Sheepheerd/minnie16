#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "bus.h"
#include "cpu.h"


typedef struct {
  CPU *cpu;
  BUS *bus;
} SIMULATOR;


SIMULATOR* sim_init();

// We must design how one interacts with the simulator. If debug, we will run in
// debug mode.

int sim_step(SIMULATOR* sim);

int debug(SIMULATOR* sim);

static int cmd_break(SIMULATOR *sim, int argc, char **argv);
static int cmd_mem(SIMULATOR *sim, int argc, char **argv);
static int cmd_regs(SIMULATOR *sim, int argc, char **argv);
static int cmd_quit(SIMULATOR *sim, int argc, char **argv);
static int cmd_help(SIMULATOR *sim, int argc, char **argv);
static int cmd_step(SIMULATOR *sim, int argc, char **argv);





#endif

