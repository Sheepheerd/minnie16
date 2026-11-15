// Browser host for the emulator, built with Emscripten (see web/build.sh).
// Stands in for display_sdl.c and tty_posix.c, and exports the small API that
// web/app.js drives. The page runs the CPU in slices from its own frame loop,
// so nothing in here blocks.

#include <emscripten.h>
#include <stdint.h>
#include <stdlib.h>

#include "debug.h"
#include "display.h"
#include "simulator.h"
#include "tty.h"

#define PROGRAM_PATH "/program.bin"
#define INPUT_SIZE 4096

// Why web_run stopped
enum {
  RUN_BUDGET, // Ran out of steps, call again
  RUN_HALTED,
  RUN_BREAK,
  RUN_FRAME, // The program wrote VID_FLIP, show the framebuffer
};

struct DISPLAY {
  int unused;
};

static SIMULATOR *sim;
static DISPLAY screen;
static int frame_ready;
static uint8_t keys;

static uint8_t input[INPUT_SIZE];
static int input_head, input_len, input_eof;

// ---- display.h ----

DISPLAY *display_open(int scale) {
  (void)scale;
  return &screen;
}

void display_close(DISPLAY *display) { (void)display; }

// The page draws straight from RAM, so presenting only has to end the slice
int display_present(DISPLAY *display, const uint8_t *fb, uint8_t *keys_out) {
  (void)display;
  (void)fb;
  frame_ready = 1;
  *keys_out = keys;
  return 0;
}

// ---- tty.h ----

EM_JS(void, js_tty_out, (int c), {
  if (Module.onTtyOut)
    Module.onTtyOut(c);
});

void tty_putc(uint8_t c) { js_tty_out(c); }

int tty_getc(void) {
  if (input_len == 0)
    return input_eof ? TTY_END : TTY_NONE;
  uint8_t c = input[input_head];
  input_head = (input_head + 1) % INPUT_SIZE;
  input_len--;
  return c;
}

// ---- API for the page ----

// Loads the image the page wrote to PROGRAM_PATH. Returns 0 on success.
EMSCRIPTEN_KEEPALIVE int web_load(void) {
  SIMULATOR *fresh = sim_init(PROGRAM_PATH);
  if (fresh == NULL)
    return -1;

  // Breakpoints are addresses, so they carry over to the new image
  if (sim != NULL) {
    fresh->num_breakpoints = sim->num_breakpoints;
    for (int i = 0; i < sim->num_breakpoints; i++)
      fresh->breakpoints[i] = sim->breakpoints[i];
    sim_free(sim);
  }
  sim = fresh;
  input_head = input_len = input_eof = 0;
  frame_ready = 0;
  return 0;
}

EMSCRIPTEN_KEEPALIVE int web_reset(void) {
  input_head = input_len = input_eof = 0;
  frame_ready = 0;
  return sim_reset(sim);
}

static int run_status(SIM_STATUS status) {
  if (status == SIM_HALTED)
    return RUN_HALTED;
  if (status == SIM_BREAK)
    return RUN_BREAK;
  return frame_ready ? RUN_FRAME : RUN_BUDGET;
}

// Runs up to max_steps instructions, stopping early on a halt, a breakpoint or
// a finished frame.
EMSCRIPTEN_KEEPALIVE int web_run(int max_steps) {
  SIM_STATUS status = SIM_OK;
  frame_ready = 0;
  for (int i = 0; i < max_steps && status == SIM_OK && !frame_ready; i++)
    status = sim_step(sim);
  return run_status(status);
}

EMSCRIPTEN_KEEPALIVE int web_step(void) {
  frame_ready = 0;
  return run_status(sim_step(sim));
}

EMSCRIPTEN_KEEPALIVE uint8_t *web_ram(void) { return sim->bus->ram; }
EMSCRIPTEN_KEEPALIVE uint16_t *web_regs(void) { return sim->cpu->reg; }
EMSCRIPTEN_KEEPALIVE int web_pc(void) { return sim->cpu->pc; }
EMSCRIPTEN_KEEPALIVE int web_halted(void) { return sim->cpu->halted; }
EMSCRIPTEN_KEEPALIVE int web_ticks(void) { return sim->bus->mmio.ticks; }

// A double holds every count the page will ever see, and avoids BigInt
EMSCRIPTEN_KEEPALIVE double web_cycles(void) {
  return (double)sim->cpu->cycles;
}

EMSCRIPTEN_KEEPALIVE void web_set_keys(int held) { keys = (uint8_t)held; }

EMSCRIPTEN_KEEPALIVE void web_tty_push(int c) {
  if (input_len == INPUT_SIZE)
    return;
  input[(input_head + input_len) % INPUT_SIZE] = (uint8_t)c;
  input_len++;
}

EMSCRIPTEN_KEEPALIVE void web_tty_eof(void) { input_eof = 1; }

static int find_breakpoint(uint16_t addr) {
  for (int i = 0; i < sim->num_breakpoints; i++)
    if (sim->breakpoints[i] == addr)
      return i;
  return -1;
}

// Returns 1 if a breakpoint is now set at addr, 0 if it was removed, -1 if
// the table is full.
EMSCRIPTEN_KEEPALIVE int web_toggle_break(int addr) {
  uint16_t a = (uint16_t)addr & 0xFFFE;
  int i = find_breakpoint(a);
  if (i >= 0) {
    sim->breakpoints[i] = sim->breakpoints[--sim->num_breakpoints];
    return 0;
  }
  if (sim->num_breakpoints == MAX_BREAKPOINTS)
    return -1;
  sim->breakpoints[sim->num_breakpoints++] = a;
  return 1;
}

EMSCRIPTEN_KEEPALIVE int web_is_break(int addr) {
  return find_breakpoint((uint16_t)addr) >= 0;
}

EMSCRIPTEN_KEEPALIVE void web_clear_breaks(void) { sim->num_breakpoints = 0; }

// Disassembles the word at addr. The string is reused by the next call.
EMSCRIPTEN_KEEPALIVE const char *web_disasm(int addr) {
  static char text[64];
  uint16_t a = (uint16_t)addr;
  uint16_t raw = bus_peek8(sim->bus, a) |
                 (uint16_t)(bus_peek8(sim->bus, (uint16_t)(a + 1)) << 8);
  disassemble(raw, a, text, sizeof(text));
  return text;
}
