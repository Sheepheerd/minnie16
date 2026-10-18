#ifndef SETUP_H
#define SETUP_H

typedef struct {
  int debug;           // Start in the step-through debugger
  const char *program; // Program image to load
} OPTIONS;

// Returns 0 to run, 1 on a bad argument, 2 if usage was printed and the
// program should exit.
int setup_opts(int argc, char *argv[], OPTIONS *opts);

#endif
