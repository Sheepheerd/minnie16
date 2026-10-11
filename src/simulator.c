#include "simulator.h"
#include "bus.h"
#include "cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 256
#define MAX_ARGS 16

typedef int (*cmd_fn)(SIMULATOR *sim, int argc, char **argv);

static int cmd_step(SIMULATOR *sim, int argc, char **argv) {
  // Run Through the program.
  if (sim == NULL) {
    printf("Simulator is not initialized");
  }

  // TODO Have a check before the halted
  if (!sim->cpu->halted) {
    cpu_step(sim->cpu, sim->bus);
  }
  return 0;
}

static int cmd_break(SIMULATOR *sim, int argc, char **argv) { return 0; }
static int cmd_mem(SIMULATOR *sim, int argc, char **argv) { return 0; }
static int cmd_regs(SIMULATOR *sim, int argc, char **argv) { return 0; }
static int cmd_quit(SIMULATOR *sim, int argc, char **argv) {
  free(sim->bus);
  free(sim->cpu);
  free(sim);
  return -1;
};
static int cmd_help(SIMULATOR *sim, int argc, char **argv) { return 0; }

typedef struct {
  const char *name;
  const char *alias;
  cmd_fn fn;
  const char *help;
} COMMAND;

static const COMMAND commands[] = {
    {"step", "s", cmd_step, "step [N]        execute N instructions"},
    {"break", "b", cmd_break, "break ADDR      set breakpoint"},
    {"mem", "x", cmd_mem, "mem ADDR [LEN]  dump memory"},
    {"regs", "r", cmd_regs, "regs            show registers"},
    {"help", "h", cmd_help, "help            list commands"},
    {"quit", "q", cmd_quit, "quit            exit"},
};
#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

SIMULATOR *sim_init() {

  SIMULATOR *sim = (SIMULATOR *)malloc(sizeof(SIMULATOR));
  CPU *cpu = cpu_init();
  BUS *bus = bus_init();
  if (sim == NULL || cpu == NULL || bus == NULL) {
    return NULL;
  }

  sim->bus = bus;
  sim->cpu = cpu;

  if (!bus_load_program(bus, "program.bin")) {
    printf("Failed to Load program into ram");
    return NULL;
  }

  return sim;
}

static int tokenize(char *line, char **argv) {
  int argc = 0;
  char *tok = strtok(line, " \t\n");
  while (tok && argc < MAX_ARGS) {
    argv[argc++] = tok;
    tok = strtok(NULL, " \t\n");
  }
  return argc;
}

static const COMMAND *find_command(const char *name) {
  for (size_t i = 0; i < NUM_COMMANDS; i++)
    if (strcmp(name, commands[i].name) == 0 ||
        (commands[i].alias && strcmp(name, commands[i].alias) == 0))
      return &commands[i];
  return NULL;
}

int debug(SIMULATOR *sim) {
  char line[MAX_LINE];
  char last[MAX_LINE] = "";
  char *argv[MAX_ARGS];

  for (;;) {
    printf("(sim) ");
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin))
      return 0;

    if (line[0] == '\n')
      strcpy(line, last);
    else
      strcpy(last, line);

    int argc = tokenize(line, argv);
    if (argc == 0)
      continue;

    const COMMAND *cmd = find_command(argv[0]);
    if (!cmd) {
      printf("unknown command: %s (try 'help')\n", argv[0]);
      continue;
    }
    if (cmd->fn(sim, argc, argv) < 0)
      return 0;
  }
}
