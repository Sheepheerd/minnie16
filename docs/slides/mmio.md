---
title: Minnie16 MMIO
sub_title: Text in, text out, and a screen to play Pong on
author: Shepard Garrett
---

Where we left off
===

The debugger can see everything. The program still can't say anything.

<!-- pause -->

To get an answer out of `fib.asm`, you stop it and read memory:

```text
(sim) mem 0x100 20
0x0100:  00 00 01 00 01 00 02 00 03 00 05 00 08 00 0D 00
```

<!-- pause -->

There are no I/O instructions, and there's no room for any. All 16 opcodes are
taken.

<!-- end_slide -->

No new instructions
===

The CPU already knows how to do two things with an address: **load** and
**store**.

<!-- pause -->

So devices pretend to be memory.

<!-- column_layout: [1, 1] -->

<!-- column: 0 -->

## Store

`SB r2, (r13)` with `r13 = 0xFF00`

A character appears on the terminal.

<!-- column: 1 -->

## Load

`LW r1, 16(r13)`

Which keys are held down right now.

<!-- reset_layout -->

<!-- pause -->

`cpu.c` didn't change at all. Everything happens in the bus.

<!-- end_slide -->

The memory map
===

```text
  0x0000 ┌──────────────────────┐
         │ RAM, program at 0    │
  0xE000 ├──────────────────────┤
         │ framebuffer  64 × 48 │
  0xEC00 ├──────────────────────┤
         │ RAM                  │
  0xFF00 ├──────────────────────┤
         │ I/O page             │
  0xFFFF └──────────────────────┘
```

<!-- pause -->

The framebuffer is **plain RAM**. Drawing is just `SB`.

The I/O page is **not** RAM. Every access there goes to a device.

<!-- end_slide -->

The bus decides
===

Every byte access checks the address first:

```c
uint8_t bus_read8(BUS *bus, uint16_t addr) {
  if (addr >= MMIO_BASE) {
    return mmio_read8(&bus->mmio, bus->ram, addr);
  }
  return bus->ram[addr];
}
```

<!-- pause -->

Word access is now built from two byte accesses:

```c
uint16_t bus_read(BUS *bus, uint16_t addr) {
  uint16_t low = bus_read8(bus, addr);
  uint16_t high = bus_read8(bus, (uint16_t)(addr + 1));
  return (uint16_t)(low | (high << 8));
}
```

So `LW`, `SW`, `LB` and `SB` all reach the devices. Nothing gets past the check.

<!-- end_slide -->

The I/O page
===

| Address | Name | | Does |
|---------|------|:-:|------|
| `0xFF00` | `TTY_OUT` | W | print a byte |
| `0xFF02` | `TTY_IN` | R | next byte from stdin, or `0` |
| `0xFF04` | `TTY_STAT` | R | bit 0 ready, bit 1 end of file |
| `0xFF10` | `KEYS` | R | keys held in the window |
| `0xFF12` | `VID_FLIP` | W | show the frame, wait for the next one |
| `0xFF14` | `TICKS` | R | frames shown since reset |

<!-- pause -->

Every register sits at an **even** address, so a word load works on all of
them. Load `r13 = 0xFF00` once, and every register is an offset from it:

```text
        LW   r1, 16(r13)
        SW   r0, 18(r13)
```

<!-- end_slide -->

Output first
===

```c
case MMIO_TTY_OUT:
  putchar(data);
  if (data == '\n')
    fflush(stdout);
  break;
```

<!-- pause -->

Printing a string means walking it with `LB` until the `0`:

```text
        LI   r13, 0xFF00
        LI   r1, msg
loop:   LB   r2, (r1)
        BEQZ r2, done
        SB   r2, (r13)
        ADDI r1, r1, 1
        JMP  loop
done:   HALT

msg:    .asciz "Hello from minnie16!\n"
```

<!-- pause -->

Two things in there the assembler couldn't do before: `LI` of a **label**, and
`.asciz`.

<!-- end_slide -->

The assembler grows up
===

Programs need data now, not just code.

| Directive | Emits |
|-----------|-------|
| `.byte 1, 2, 0x41` | one byte each |
| `.word 0x1234, label` | one word each, aligned first |
| `.ascii "text"` | the bytes |
| `.asciz "text"` | the bytes, then `0` |
| `.space 64` | that many zeros |
| `.align` | a zero if the address is odd |

<!-- pause -->

`LI` used to expand into numbers right away. Now it waits for pass two, so it
can load a label defined later in the file.

<!-- pause -->

A string can have an odd length, so an instruction after data gets a `0` byte
of padding first. Instructions stay word aligned.

<!-- end_slide -->

Hello
===

Press `ctrl+e`:

```bash +exec
cd "$(git rev-parse --show-toplevel)"
cmake -B build -S . >/dev/null && cmake --build build >/dev/null
python3 tools/m16asm.py programs/hello.asm build/hello.bin >/dev/null
build/minnie -n build/hello.bin | head -1
```

<!-- // output: Hello from minnie16! -->

<!-- pause -->

The program said something without being stopped.

<!-- end_slide -->

Input never blocks
===

`TTY_IN` returns a byte if one is waiting, and `0` if not. It never waits.

```c
static uint8_t tty_status(MMIO *mmio) {
  if (!mmio->pending && !mmio->eof) {
    struct pollfd pfd = {.fd = STDIN_FILENO, .events = POLLIN};
    if (poll(&pfd, 1, 0) > 0) {
      ...
    }
  }
  ...
}
```

<!-- pause -->

`poll` with a timeout of `0` asks "is anything there?" without waiting.

<!-- pause -->

If the CPU blocked on stdin, Pong would freeze until you typed something. So the
program polls `TTY_STAT` instead, and decides for itself when to wait.

<!-- end_slide -->

Reading a line
===

<!-- column_layout: [1, 1] -->

<!-- column: 0 -->

```text
wait:   LB   r2, (r10)
        ADDI r3, r0, 1
        AND  r3, r2, r3
        BNEZ r3, got
        BEQZ r2, wait
        JMP  done
got:    LB   r2, (r11)
        SUB  r3, r2, r12
        BEQZ r3, done
        ...
```

<!-- column: 1 -->

`r10` is `TTY_STAT`, `r11` is `TTY_IN`, `r12` is `'\n'`.

- Bit 0 set: read the byte.
- Nothing set: ask again.
- Bit 1 set: end of file, stop.

<!-- reset_layout -->

<!-- pause -->

Without the end-of-file bit, `minnie greet.bin < /dev/null` would spin forever.

<!-- pause -->

```bash +exec
cd "$(git rev-parse --show-toplevel)"
cmake -B build -S . >/dev/null && cmake --build build >/dev/null
python3 tools/m16asm.py programs/greet.asm build/greet.bin >/dev/null
printf 'Shepard\n' | build/minnie -n build/greet.bin | head -1
```

<!-- // output: What's your name? Hello, Shepard! -->

<!-- end_slide -->

A screen
===

64 × 48 pixels, one byte each, row by row:

```text
addr = 0xE000 + y × 64 + x
```

<!-- pause -->

There's no multiply. But 64 is `2^6`:

```text
        ADDI r6, r0, 6
        SLL  r7, r2, r6
        ADD  r7, r7, r1
        ADD  r7, r7, r12
        SB   r5, (r7)
```

<!-- pause -->

The width was picked so the hardware we already have can do the math.

<!-- end_slide -->

Sixteen colors
===

The low 4 bits of each pixel pick from a fixed palette:

| | | | |
|---|---|---|---|
| `0` black | `4` red | `8` dark gray | `12` light red |
| `1` blue | `5` magenta | `9` light blue | `13` light magenta |
| `2` green | `6` brown | `10` light green | `14` yellow |
| `3` cyan | `7` light gray | `11` light cyan | `15` white |

<!-- pause -->

The window is SDL2, scaled up 10× to 640 × 480. The emulator copies the
framebuffer through the palette into a texture, and SDL stretches it.

<!-- end_slide -->

VID_FLIP
===

Writing anything to `0xFF12` does three things:

1. Copies the framebuffer to the window.
2. Waits until 1/60 of a second has passed since the last flip.
3. Reads the keyboard into `KEYS`, and adds one to `TICKS`.

<!-- pause -->

That one store is the program's whole frame loop:

```text
frame:  ...
        SW   r0, 18(r13)
        JAL  frame
```

<!-- pause -->

The CPU runs as fast as it can between flips. The flip is what sets the game
speed, not the clock.

<!-- end_slide -->

The window is lazy
===

The window only opens on the **first** `VID_FLIP`.

<!-- pause -->

<!-- column_layout: [1, 1] -->

<!-- column: 0 -->

## Text programs

`hello`, `greet`, `fib` never flip, so they never get a window.

<!-- column: 1 -->

## `-n`, `--headless`

Never open one. Flips still count `TICKS`, but don't wait.

<!-- reset_layout -->

<!-- pause -->

Headless is how Pong got tested here, with no screen at all. Set `KEYS` by
hand, run 3000 frames, print the framebuffer as ASCII.

<!-- pause -->

No SDL2 installed? CMake builds `display_null.c` instead, and every run is
headless.

<!-- end_slide -->

Keys
===

| Bit | Key |
|:---:|-----|
| 0 | W |
| 1 | S |
| 2 | Up |
| 3 | Down |
| 4 | Space |
| 5 | Escape |

<!-- pause -->

`KEYS` is what's held **right now**, not a queue of presses. It's sampled once
per flip, so it can't change in the middle of a frame.

<!-- pause -->

Closing the window halts the CPU. `sim_step` checks a `quit` flag after every
instruction.

<!-- end_slide -->

Pong
===

Every frame:

1. Read `KEYS`. Escape halts.
2. Move both paddles.
3. Move the ball, unless it's waiting for Space.
4. Bounce off walls and paddles. Score if it reaches an edge.
5. Clear the screen, draw the net, the paddles and the ball.
6. `VID_FLIP`.

<!-- pause -->

Game state lives in RAM at `0x8000`, reached through `r14`:

| Offset | | Offset | |
|---|---|---|---|
| `0` | left paddle y | `8` | ball dx |
| `2` | right paddle y | `10` | ball dy |
| `4` | ball x | `12`, `14` | scores |
| `6` | ball y | `16` | waiting to serve |

<!-- end_slide -->

Hitting the paddle
===

The ball hits if `paddle_y ≤ ball_y < paddle_y + 8`. That's two compares.

<!-- pause -->

Or one:

```text
        LW   r7, 0(r14)
        SUB  r7, r3, r7
        SLT  r7, r7, r8
        BEQZ r7, noleft
        ADDI r4, r0, 1
```

<!-- pause -->

If the ball is above the paddle, `ball_y − paddle_y` is negative, which wraps to
something like `0xFFFE`. `SLT` is **unsigned**, so `0xFFFE < 8` is false.

One subtraction and one unsigned compare check both ends.

<!-- end_slide -->

Clearing the screen
===

3072 bytes, every frame. The loop is unrolled 16 words at a time:

```text
cloop:  SW   r0, 0(r1)
        SW   r0, 2(r1)
        ...
        SW   r0, 30(r1)
        ADD  r1, r1, r3
        ADDI r2, r2, -1
        BNEZ r2, cloop
```

<!-- pause -->

16 stores is exactly what `SW`'s 4-bit word offset can reach: 0 to 30 bytes.
96 trips instead of 1536.

<!-- pause -->

A whole Pong frame is about **2500 instructions**.

<!-- end_slide -->

Serving
===

There's no random number generator. But there is a clock:

```text
        LW   r2, 20(r13)
        LIL  r3, 31
        AND  r3, r2, r3
        ADDI r3, r3, 7
```

<!-- pause -->

The ball starts at a height from `TICKS`, which depends on how long you waited
to press Space. Bit 5 of `TICKS` picks up or down.

<!-- pause -->

The score goes to the terminal, not the screen. The screen has no font, but
`TTY_OUT` does:

```text
Left 1  Right 0
Left 1  Right 1
```

<!-- end_slide -->

Play it
===

```bash
python3 tools/m16asm.py programs/pong.asm pong.bin
./build/minnie pong.bin
```

| Key | |
|-----|---|
| W / S | left paddle |
| Up / Down | right paddle |
| Space | serve |
| Escape | quit |

<!-- pause -->

Headless, it waits for Space forever, so all you get is the title:

```bash +exec
cd "$(git rev-parse --show-toplevel)"
cmake -B build -S . >/dev/null && cmake --build build >/dev/null
python3 tools/m16asm.py programs/pong.asm build/pong.bin >/dev/null
timeout 1 build/minnie -n build/pong.bin | head -1
```

<!-- // output: PONG  left: W/S  right: Up/Down  space: serve  esc: quit -->

<!-- end_slide -->

Summary
===

The CPU didn't change. The bus learned to say "that address isn't RAM".

| Piece | Gives you |
|-------|-----------|
| `TTY_OUT` | output without stopping the program |
| `TTY_IN` + `TTY_STAT` | input that never blocks |
| Framebuffer | a 64 × 48 screen that's just memory |
| `VID_FLIP` | a frame loop and a speed limit |
| `KEYS` + `TICKS` | controls, and a source of randomness |
| Directives | strings and data in the program |

<!-- end_slide -->

What's Next
===

The roadmap so far:

- [x] Assembler
- [x] Step by step debugger
- [x] MMIO
- [x] Visual display

<!-- pause -->

Next is a **web frontend**: the same emulator in a browser, with the console
output and the screen side by side.
