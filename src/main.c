#include "setup.h"
#include "simulator.h"
#include <getopt.h>
#include <stdio.h>

static void usage(const char *name) {
  printf("Usage: %s [-d|--debug] [program.bin]\n"
         "  -d, --debug  start in the step-through debugger\n"
         "  -h, --help   show this help\n",
         name);
}

int main(int argc, char *argv[]) {
  OPTIONS opts;

  switch (setup_opts(argc, argv, &opts)) {
  case 1:
    fprintf(stderr, "Error: Couldn't parse arguments. Use -h for usage\n");
    return 1;
  case 2:
    return 0;
  }

  SIMULATOR *sim = sim_init(opts.program);
  if (sim == NULL) {
    return 1;
  }

  int status = 0;
  if (opts.debug) {
    debug(sim);
  } else if (sim_run(sim) != SIM_HALTED) {
    status = 1;
  } else {
    printf("Halted at 0x%04X after %llu instructions\n", sim->cpu->pc,
           (unsigned long long)sim->cpu->cycles);
    sim_print_regs(sim);
  }

  sim_free(sim);
  return status;
}

int setup_opts(int argc, char *argv[], OPTIONS *opts) {
  opts->debug = 0;
  opts->program = "program.bin";

  static struct option long_opts[] = {
      {"debug", no_argument, 0, 'd'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "dh", long_opts, NULL)) != -1) {
    switch (opt) {
    case 'd':
      opts->debug = 1;
      break;
    case 'h':
      usage(argv[0]);
      return 2;
    default:
      return 1;
    }
  }

  if (optind < argc) {
    opts->program = argv[optind++];
  }
  if (optind < argc) {
    return 1; // Only one program at a time
  }
  return 0;
}
