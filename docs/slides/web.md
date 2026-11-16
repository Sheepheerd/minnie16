---
title: Minnie16 Web Frontend
sub_title: The same emulator, one link away
author: Shepard Garrett
---

Where we left off
===

Pong runs. Showing it to someone is the hard part.

<!-- pause -->

To try minnie16 today, you need:

1. A clone of the repository
2. CMake and a C compiler
3. SDL2, or there's no window
4. Python, for the assembler

<!-- pause -->

It should be a **link**.

<!-- end_slide -->

Two ways to get there
===

<!-- column_layout: [1, 1] -->

<!-- column: 0 -->

## Rewrite it

Write the CPU again in JavaScript.

Now there are two CPUs. Every fix has to be made twice, and one day they
disagree.

<!-- column: 1 -->

## Compile it

Build the C we already have for the browser.

One CPU. If it's right in the terminal, it's right on the page.

<!-- reset_layout -->

<!-- pause -->

We compile it. **Emscripten** turns C into WebAssembly, and gives it a small
JavaScript file to load it with.

<!-- end_slide -->

What gets compiled
===

```text
  cpu.c  bus.c  decoder.c  simulator.c  debug.c  mmio.c
  ─────────────────────────────────────────────────────
                    the same six files
  ─────────────────────────────────────────────────────
      native                          browser
      main.c                          web.c
      display_sdl.c
      tty_posix.c
```

<!-- pause -->

The whole browser side of the emulator is **one file**, `src/web.c`.

<!-- pause -->

```bash
emcc -O2 src/cpu.c src/bus.c ... src/web.c -o build/web/minnie.js
```

Out comes `minnie.wasm`, about 25 KiB.

<!-- end_slide -->

What the emulator asks of its host
===

The CPU and the bus never touch the outside world. Only the devices do.

<!-- pause -->

The screen was already behind an interface. That's how `display_null.c` could
stand in for `display_sdl.c`:

```c
DISPLAY *display_open(int scale);
void display_close(DISPLAY *display);
int display_present(DISPLAY *display, const uint8_t *fb, uint8_t *keys);
```

<!-- pause -->

The terminal wasn't. `mmio.c` called `poll`, `read` and `putchar` itself, and
a browser tab has no stdin.

<!-- end_slide -->

Giving the terminal an interface
===

The same trick, one more time. `src/tty.h`:

```c
#define TTY_NONE (-1) // Nothing waiting right now
#define TTY_END (-2)  // Input reached end of file

void tty_putc(uint8_t c);
int tty_getc(void);
```

<!-- pause -->

The `poll` code moved to `tty_posix.c`, unchanged. `mmio.c` got shorter:

```c
int c = tty_getc();
if (c >= 0) {
  mmio->buffered = (uint8_t)c;
  mmio->pending = 1;
} else if (c == TTY_END) {
  mmio->eof = 1;
}
```

<!-- pause -->

This is the **only** change to the existing emulator.

<!-- end_slide -->

A tab can't wait
===

Natively, `VID_FLIP` sleeps until the next frame. `sim_run` loops until `HALT`.

<!-- pause -->

In a browser, both of those freeze the page. JavaScript has to get control
back, or nothing is drawn and no key is ever read.

<!-- pause -->

So the page runs the CPU in **slices**:

```c
EMSCRIPTEN_KEEPALIVE int web_run(int max_steps) {
  SIM_STATUS status = SIM_OK;
  frame_ready = 0;
  for (int i = 0; i < max_steps && status == SIM_OK && !frame_ready; i++)
    status = sim_step(sim);
  return run_status(status);
}
```

A slice ends on a halt, a breakpoint, a finished frame, or when it runs out of
steps.

<!-- end_slide -->

The screen that does nothing
===

`web.c` implements `display.h` in a few lines:

```c
int display_present(DISPLAY *display, const uint8_t *fb, uint8_t *keys_out) {
  frame_ready = 1;
  *keys_out = keys;
  return 0;
}
```

<!-- pause -->

It doesn't draw. It ends the slice and hands over the keys.

<!-- pause -->

The framebuffer is plain RAM, and the page can see the emulator's RAM. So the
page draws it by itself:

```js
const ram = m._web_ram();
const fb = m.HEAPU8.subarray(ram + FB_BASE, ram + FB_BASE + FB_SIZE);
```

3072 bytes go through the same 16 color palette into a `<canvas>`.

<!-- end_slide -->

The frame loop moved
===

The wait that `VID_FLIP` used to do is now the browser's job:

```js
function tick(now) {
  if (!running) return;
  requestAnimationFrame(tick);
  if (now < nextFrame - 2) return;
  nextFrame = Math.max(nextFrame + FRAME_MS, now);

  const status = m._web_run(STEPS_PER_FRAME);
  ...
  drawScreen();
}
```

<!-- pause -->

`requestAnimationFrame` fires once per monitor refresh. On a 144 Hz monitor
that would make Pong more than twice as fast, so the loop skips refreshes to
stay at **60 per second**.

<!-- pause -->

A program that never flips gets **400,000 instructions** per slice.

<!-- end_slide -->

The console
===

<!-- column_layout: [1, 1] -->

<!-- column: 0 -->

## Out

`tty_putc` calls into JavaScript:

```c
EM_JS(void, js_tty_out, (int c), {
  if (Module.onTtyOut)
    Module.onTtyOut(c);
});
```

The page collects the bytes and shows them once per frame.

<!-- column: 1 -->

## In

Enter pushes the line into a buffer:

```c
int tty_getc(void) {
  if (input_len == 0)
    return input_eof ? TTY_END
                     : TTY_NONE;
  ...
}
```

Ctrl+D sets end of file.

<!-- reset_layout -->

<!-- pause -->

`TTY_IN` never blocks, which was needed for Pong. It's also what makes
`greet.asm` work here without changes. It polls `TTY_STAT`, the slice runs out,
the page stays alive, and you get to type.

<!-- end_slide -->

Loading a program
===

`sim_init` takes a **path**, and `reset` reads the file again. There are no
files in a browser.

<!-- pause -->

Emscripten has a filesystem in memory. The page writes the image there:

```js
m.FS.writeFile("/program.bin", result.image);
m._web_load();
```

<!-- pause -->

```c
EMSCRIPTEN_KEEPALIVE int web_load(void) {
  SIMULATOR *fresh = sim_init(PROGRAM_PATH);
  ...
}
```

`bus_load_program` still calls `fopen`. It doesn't know the file is fake, and
`reset` works the way it always did.

<!-- end_slide -->

The assembler is the exception
===

The assembler is Python. A browser can't run it without downloading a whole
Python interpreter.

<!-- pause -->

So `web/asm.js` is a port of `tools/m16asm.py`, function for function.

<!-- pause -->

That's the "two copies" problem we just avoided for the CPU. The fix is a test
that assembles every example with **both** and compares the bytes:

```bash +exec
cd "$(git rev-parse --show-toplevel)"
node web/test/asm.test.js | head -5
```

<!-- // output:
ok    example.asm
ok    fib.asm
ok    greet.asm
ok    hello.asm
ok    pong.asm
-->

<!-- pause -->

Change one assembler and forget the other, and this fails.

<!-- end_slide -->

The debugger, with buttons
===

Everything the `(sim)` prompt could do was already a function.

| Terminal | Page | Underneath |
|----------|------|------------|
| `step` | **Step** | `sim_step` |
| `continue` | **Run** / **Pause** | `sim_step` in slices |
| `break ADDR` | click a line | `sim->breakpoints` |
| `regs` | Registers | a pointer to `cpu->reg` |
| `mem` | Memory | a pointer to `bus->ram` |
| `disas` | Disassembly | `disassemble` |
| `reset` | **Reset** | `sim_reset` |

<!-- pause -->

The disassembly text comes from `debug.c`, so it reads exactly like the
terminal.

<!-- end_slide -->

One thing the terminal couldn't do
===

The native debugger only has the `.bin`. The source is gone by then.

<!-- pause -->

The page has both. `asm.js` returns two extra things with the image:

```js
return {
  image: image.slice(0, size),
  labels,
  lines,
  instructions: instrs.length,
};
```

<!-- pause -->

- `lines` maps an address to a source line, so the editor highlights the line
  the pc is on.
- `labels` puts the names back into the disassembly:

```text
       0x000A  F000  HALT
       fib:
       0x000C  0200  MOV  r2, r0
```

<!-- end_slide -->

Testing it without a browser
===

The WebAssembly build also loads in Node. The same module, with no page:

```bash +exec
cd "$(git rev-parse --show-toplevel)"
web/build.sh >/dev/null 2>&1
node web/test/sim.test.js | sed -n '5,10p'
```

<!-- // output:
ok    greet waits for input
ok    greet halts
ok    greet answers
ok    pong shows a frame
ok    pong draws something
ok    pong quits on escape
-->

<!-- pause -->

Pong is tested the way it was in headless mode: hold a key with
`web_set_keys`, run some frames, look at the framebuffer.

<!-- end_slide -->

Publishing
===

GitHub Pages serves static files, and that's all this is.

| File | Size |
|------|-----:|
| `minnie.wasm` | 25 KiB |
| `minnie.js` | 58 KiB |
| `app.js` + `asm.js` | 28 KiB |
| `index.html` + `style.css` | 9 KiB |

<!-- pause -->

On every push to `main`, a workflow:

1. Checks `asm.js` against `m16asm.py`
2. Builds with Emscripten
3. Runs the example programs on the build
4. Deploys

<!-- pause -->

If a test fails, the old page stays up.

<!-- end_slide -->

Try it
===

<https://sheepheerd.github.io/minnie16/>

<!-- pause -->

1. Pick `pong.asm` from the **Example** menu
2. Press **Run**
3. Click the display, then:

| Key | |
|-----|---|
| W / S | left paddle |
| Up / Down | right paddle |
| Space | serve |
| Escape | quit |

<!-- pause -->

Or build it yourself:

```bash
web/build.sh
python3 -m http.server -d build/web 8000
```

<!-- end_slide -->

Summary
===

The emulator didn't change. It got a second host.

| Piece | Gives you |
|-------|-----------|
| Emscripten | the C emulator, running in a tab |
| `tty.h` | a terminal that isn't stdin and stdout |
| `web.c` | a screen, a console and an API in one file |
| `web_run` | a CPU that runs in slices and never blocks |
| `asm.js` | the assembler, with a test that keeps it honest |
| GitHub Pages | a link instead of a build |

<!-- end_slide -->

What's Next
===

The roadmap so far:

- [x] Assembler
- [x] Step by step debugger
- [x] MMIO
- [x] Visual display
- [x] Web frontend

<!-- pause -->

Last is a **Ghidra processor module**: teaching a real reverse engineering
tool to read minnie16 binaries.
