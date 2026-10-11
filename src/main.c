#include "cpu.h"
#include "setup.h"
#include "simulator.h"
#include <getopt.h>
#include <stdio.h>

int main(int argc, char *argv[]) {

  if (setup_opts(argc, argv) == 1) {
    printf(
        "Error: Couldn't parse correct arguments. Please use -h for usage\n");
  };

  SIMULATOR *sim = sim_init();

  debug(sim);

  return 0;
}

int setup_opts(int argc, char *argv[]) {
  // Set up options
  int debug = 0;

  static struct option long_opts[] = {
      {"debug", no_argument, 0, 'd'},
  };

  int opt;

  while ((opt = getopt_long(argc, argv, "vo:c:h", long_opts, NULL)) != -1) {
    switch (opt) {
    case 'd':
      debug = 1;
      break;
    case 'h':
      printf("Usage: %s [--debug] [files...]\n", argv[0]);
      return 0;
    default:
      return 1;
    }
  }
}
