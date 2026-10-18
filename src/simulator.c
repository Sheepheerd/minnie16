#include "simulator.h"
#include "bus.h"
#include "cpu.h"
#include "debug.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINE 256
#define MAX_ARGS 16

// Set by ctrl+c so a runaway 'continue' drops back to the prompt
static volatile sig_atomic_t interrupted = 0;

static void on_sigint(int sig) {
  (void)sig;
  interrupted = 1;
}

SIMULATOR *sim_init(const char *path) {
  SIMULATOR *sim = (SIMULATOR *)calloc(1, sizeof(SIMULATOR));
  if (sim == NULL) {
    return NULL;
  }

  sim->cpu = cpu_init();
  sim->bus = bus_init();
  sim->program = path;
  if (sim->cpu == NULL || sim->bus == NULL || sim_reset(sim) != 0) {
    sim_free(sim);
    return NULL;
  }

  return sim;
}

void sim_free(SIMULATOR *sim) {
  if (sim == NULL) {
    return;
  }
  free(sim->bus);
  free(sim->cpu);
  free(sim);
}

int sim_reset(SIMULATOR *sim) {
  cpu_reset(sim->cpu);

  int loaded = bus_load_program(sim->bus, sim->program);
  if (loaded < 0) {
    fprintf(stderr, "Failed to load %s (missing, or larger than 64 KiB)\n",
            sim->program);
    return -1;
  }
  if (loaded == 0) {
    fprintf(stderr, "Failed to load %s: file is empty\n", sim->program);
    return -1;
  }
  return 0;
}

static int find_breakpoint(SIMULATOR *sim, uint16_t addr) {
  for (int i = 0; i < sim->num_breakpoints; i++)
    if (sim->breakpoints[i] == addr)
      return i;
  return -1;
}

SIM_STATUS sim_step(SIMULATOR *sim) {
  if (sim->cpu->halted) {
    return SIM_HALTED;
  }

  cpu_step(sim->cpu, sim->bus);

  if (sim->cpu->halted) {
    return SIM_HALTED;
  }
  if (find_breakpoint(sim, sim->cpu->pc) >= 0) {
    return SIM_BREAK;
  }
  return SIM_OK;
}

SIM_STATUS sim_run(SIMULATOR *sim) {
  SIM_STATUS status;
  interrupted = 0;
  do {
    status = sim_step(sim);
  } while (status == SIM_OK && !interrupted);

  if (interrupted) {
    interrupted = 0;
    return SIM_BREAK;
  }
  return status;
}

// Parses a number in decimal, hex (0x) or octal (0) that fits in 16 bits
static int parse_u16(const char *s, uint16_t *out) {
  char *end;
  long v = strtol(s, &end, 0);
  if (*s == '\0' || *end != '\0' || v < -0x8000 || v > 0xFFFF) {
    printf("bad number: %s\n", s);
    return -1;
  }
  *out = (uint16_t)v;
  return 0;
}

// Parses "r0".."r15", returning -1 if s isn't a register name
static int parse_reg(const char *s) {
  if (s[0] != 'r' && s[0] != 'R')
    return -1;
  char *end;
  long n = strtol(s + 1, &end, 10);
  if (s[1] == '\0' || *end != '\0' || n < 0 || n > 15)
    return -1;
  return (int)n;
}

// Prints one line of disassembly, marking the pc and breakpoints
static void print_inst(SIMULATOR *sim, uint16_t addr) {
  char text[64];
  uint16_t raw = bus_read(sim->bus, addr);
  disassemble(raw, addr, text, sizeof(text));
  printf("%s%c 0x%04X:  %04X   %s\n", addr == sim->cpu->pc ? "=>" : "  ",
         find_breakpoint(sim, addr) >= 0 ? '*' : ' ', addr, raw, text);
}

// Tells the user why execution stopped and where it is now
static void report(SIMULATOR *sim, SIM_STATUS status) {
  if (status == SIM_HALTED) {
    printf("Halted at 0x%04X after %llu instructions\n", sim->cpu->pc,
           (unsigned long long)sim->cpu->cycles);
    return;
  }
  if (status == SIM_BREAK) {
    int i = find_breakpoint(sim, sim->cpu->pc);
    if (i >= 0)
      printf("Breakpoint %d at 0x%04X\n", i + 1, sim->cpu->pc);
    else
      printf("Interrupted\n");
  }
  print_inst(sim, sim->cpu->pc);
}

typedef int (*cmd_fn)(SIMULATOR *sim, int argc, char **argv);

static int cmd_step(SIMULATOR *sim, int argc, char **argv) {
  uint16_t n = 1;
  if (argc > 1 && parse_u16(argv[1], &n) != 0)
    return 0;

  if (sim->cpu->halted) {
    printf("CPU is halted (use 'reset' to start over)\n");
    return 0;
  }

  SIM_STATUS status = SIM_OK;
  for (uint16_t i = 0; i < n && status == SIM_OK; i++)
    status = sim_step(sim);
  report(sim, status);
  return 0;
}

static int cmd_continue(SIMULATOR *sim, int argc, char **argv) {
  (void)argc;
  (void)argv;
  if (sim->cpu->halted) {
    printf("CPU is halted (use 'reset' to start over)\n");
    return 0;
  }
  report(sim, sim_run(sim));
  return 0;
}

static int cmd_break(SIMULATOR *sim, int argc, char **argv) {
  if (argc < 2) {
    if (sim->num_breakpoints == 0)
      printf("No breakpoints\n");
    for (int i = 0; i < sim->num_breakpoints; i++)
      printf("%2d  0x%04X\n", i + 1, sim->breakpoints[i]);
    return 0;
  }

  uint16_t addr;
  if (parse_u16(argv[1], &addr) != 0)
    return 0;
  if (addr & 1) {
    printf("Instructions are word aligned, 0x%04X is odd\n", addr);
    return 0;
  }
  if (find_breakpoint(sim, addr) >= 0) {
    printf("Breakpoint already set at 0x%04X\n", addr);
    return 0;
  }
  if (sim->num_breakpoints == MAX_BREAKPOINTS) {
    printf("Too many breakpoints (max %d)\n", MAX_BREAKPOINTS);
    return 0;
  }

  sim->breakpoints[sim->num_breakpoints++] = addr;
  printf("Breakpoint %d at 0x%04X\n", sim->num_breakpoints, addr);
  return 0;
}

static int cmd_delete(SIMULATOR *sim, int argc, char **argv) {
  if (argc < 2) {
    sim->num_breakpoints = 0;
    printf("Deleted all breakpoints\n");
    return 0;
  }

  uint16_t n;
  if (parse_u16(argv[1], &n) != 0)
    return 0;
  if (n < 1 || n > sim->num_breakpoints) {
    printf("No breakpoint %d (see 'break')\n", n);
    return 0;
  }

  // Shift the rest down so the numbers stay contiguous
  for (int i = n - 1; i < sim->num_breakpoints - 1; i++)
    sim->breakpoints[i] = sim->breakpoints[i + 1];
  sim->num_breakpoints--;
  printf("Deleted breakpoint %d\n", n);
  return 0;
}

void sim_print_regs(SIMULATOR *sim) {
  CPU *cpu = sim->cpu;
  printf("pc  0x%04X   instructions %llu%s\n", cpu->pc,
         (unsigned long long)cpu->cycles, cpu->halted ? "   (halted)" : "");
  for (int i = 0; i < 16; i++) {
    printf("r%-2d 0x%04X %6d%s", i, cpu->reg[i], (int16_t)cpu->reg[i],
           i % 4 == 3 ? "\n" : "   ");
  }
}

static int cmd_regs(SIMULATOR *sim, int argc, char **argv) {
  (void)argc;
  (void)argv;
  sim_print_regs(sim);
  return 0;
}

static int cmd_mem(SIMULATOR *sim, int argc, char **argv) {
  if (argc < 2) {
    printf("usage: mem ADDR [LEN]\n");
    return 0;
  }

  uint16_t addr, len = 64;
  if (parse_u16(argv[1], &addr) != 0)
    return 0;
  if (argc > 2 && parse_u16(argv[2], &len) != 0)
    return 0;

  for (uint16_t row = 0; row < len; row += 16) {
    uint16_t base = addr + row;
    int count = len - row < 16 ? len - row : 16;
    printf("0x%04X: ", base);
    for (int i = 0; i < 16; i++) {
      if (i < count)
        printf(" %02X", bus_read8(sim->bus, base + i));
      else
        printf("   ");
    }
    printf("  ");
    for (int i = 0; i < count; i++) {
      uint8_t c = bus_read8(sim->bus, base + i);
      putchar(c >= 0x20 && c < 0x7F ? c : '.');
    }
    putchar('\n');
    if (row > 0xFFFF - 16)
      break; // Don't wrap the row counter
  }
  return 0;
}

static int cmd_disas(SIMULATOR *sim, int argc, char **argv) {
  uint16_t addr = sim->cpu->pc, n = 8;
  if (argc > 1 && parse_u16(argv[1], &addr) != 0)
    return 0;
  if (argc > 2 && parse_u16(argv[2], &n) != 0)
    return 0;

  addr &= 0xFFFE;
  for (uint16_t i = 0; i < n; i++, addr += 2)
    print_inst(sim, addr);
  return 0;
}

static int cmd_set(SIMULATOR *sim, int argc, char **argv) {
  if (argc < 3) {
    printf("usage: set REG|pc|ADDR VALUE\n");
    return 0;
  }

  uint16_t value;
  if (parse_u16(argv[2], &value) != 0)
    return 0;

  int r = parse_reg(argv[1]);
  if (strcmp(argv[1], "pc") == 0) {
    sim->cpu->pc = value & 0xFFFE;
    print_inst(sim, sim->cpu->pc);
  } else if (r == 0) {
    printf("r0 is hardwired to zero\n");
  } else if (r > 0) {
    sim->cpu->reg[r] = value;
    printf("r%d = 0x%04X\n", r, value);
  } else {
    uint16_t addr;
    if (parse_u16(argv[1], &addr) != 0)
      return 0;
    bus_write(sim->bus, addr, value);
    printf("MEM16[0x%04X] = 0x%04X\n", addr, value);
  }
  return 0;
}

static int cmd_reset(SIMULATOR *sim, int argc, char **argv) {
  (void)argc;
  (void)argv;
  if (sim_reset(sim) == 0) {
    printf("Reloaded %s\n", sim->program);
    print_inst(sim, sim->cpu->pc);
  }
  return 0;
}

static int cmd_quit(SIMULATOR *sim, int argc, char **argv) {
  (void)sim;
  (void)argc;
  (void)argv;
  return -1;
}

static int cmd_help(SIMULATOR *sim, int argc, char **argv);

typedef struct {
  const char *name;
  const char *alias;
  cmd_fn fn;
  const char *help;
} COMMAND;

static const COMMAND commands[] = {
    {"step", "s", cmd_step, "step [N]           execute N instructions"},
    {"continue", "c", cmd_continue, "continue           run to a breakpoint or halt"},
    {"break", "b", cmd_break, "break [ADDR]       set a breakpoint, or list them"},
    {"delete", "d", cmd_delete, "delete [N]         delete breakpoint N, or all"},
    {"regs", "r", cmd_regs, "regs               show registers"},
    {"mem", "x", cmd_mem, "mem ADDR [LEN]     dump memory"},
    {"disas", "l", cmd_disas, "disas [ADDR] [N]   disassemble N instructions"},
    {"set", NULL, cmd_set, "set REG|ADDR VAL   write a register, pc or word"},
    {"reset", NULL, cmd_reset, "reset              reload the program"},
    {"help", "h", cmd_help, "help               list commands"},
    {"quit", "q", cmd_quit, "quit               exit"},
};
#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

static int cmd_help(SIMULATOR *sim, int argc, char **argv) {
  (void)sim;
  (void)argc;
  (void)argv;
  for (size_t i = 0; i < NUM_COMMANDS; i++) {
    printf("  %-3s %s\n", commands[i].alias ? commands[i].alias : "",
           commands[i].help);
  }
  printf("Numbers can be decimal or hex (0x). Enter repeats the last command.\n");
  return 0;
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
  // Echo commands read from a pipe so scripted sessions read like typed ones
  int echo = !isatty(STDIN_FILENO);

  signal(SIGINT, on_sigint);
  printf("Loaded %s. Type 'help' for commands.\n", sim->program);
  print_inst(sim, sim->cpu->pc);

  for (;;) {
    printf("(sim) ");
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin)) {
      putchar('\n');
      break;
    }
    if (echo)
      fputs(line, stdout);

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
      break;
  }

  signal(SIGINT, SIG_DFL);
  return 0;
}
