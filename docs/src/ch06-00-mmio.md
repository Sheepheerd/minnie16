# Memory-mapped I/O

Minnie16 has no I/O instructions. Devices are reached with ordinary loads and
stores to fixed addresses. The bus in `src/bus.c` hands every access at or
above `0xFF00` to `src/mmio.c` instead of RAM.

## Memory map

| Range             | Use                                    |
|-------------------|----------------------------------------|
| `0x0000`–`0xDFFF` | RAM. Programs load at `0x0000`.        |
| `0xE000`–`0xEBFF` | Framebuffer, 64 × 48 bytes. Plain RAM. |
| `0xEC00`–`0xFEFF` | RAM.                                   |
| `0xFF00`–`0xFFFF` | I/O page.                              |

## Device registers

| Address  | Name       | Access | Meaning |
|----------|------------|:------:|---------|
| `0xFF00` | `TTY_OUT`  | W      | Writes the low byte to the terminal (stdout). |
| `0xFF02` | `TTY_IN`   | R      | Next byte from stdin, or `0` if none is waiting. Reading consumes it. |
| `0xFF04` | `TTY_STAT` | R      | Bit 0: a byte is waiting. Bit 1: stdin reached end of file. |
| `0xFF10` | `KEYS`     | R      | Keys held down in the graphics window, see below. |
| `0xFF12` | `VID_FLIP` | W      | Shows the framebuffer and waits for the next frame (about 60 per second). |
| `0xFF14` | `TICKS`    | R      | 16-bit count of frames shown since reset. |

All other addresses in the I/O page read as `0` and ignore writes. Each
register is a byte or a word at an even address, so `LB`/`SB` and `LW`/`SW`
both work. A word store to `TTY_OUT` only prints the low byte.

`TTY_IN` never blocks. A program that wants a line of input polls `TTY_STAT`
until bit 0 or bit 1 is set. When stdin is a terminal, the terminal still
buffers by line, so bytes arrive once Enter is pressed.

### Keys

| Bit | Key    |
|:---:|--------|
| 0   | W      |
| 1   | S      |
| 2   | Up     |
| 3   | Down   |
| 4   | Space  |
| 5   | Escape |

The key state is sampled on every `VID_FLIP`, so it only changes once per
frame.

## Graphics

The framebuffer is 64 pixels wide and 48 tall, one byte per pixel, row by
row. The pixel at `(x, y)` is at `0xE000 + y × 64 + x`, which is
`0xE000 + (y << 6) + x`. The low 4 bits pick a color:

| Value | Color      | Value | Color         |
|:-----:|------------|:-----:|---------------|
| 0     | black      | 8     | dark gray     |
| 1     | blue       | 9     | light blue    |
| 2     | green      | 10    | light green   |
| 3     | cyan       | 11    | light cyan    |
| 4     | red        | 12    | light red     |
| 5     | magenta    | 13    | light magenta |
| 6     | brown      | 14    | yellow        |
| 7     | light gray | 15    | white         |

The window opens the first time a program writes `VID_FLIP`, so text-only
programs never open one. It is scaled up 10 times to 640 × 480. Closing the
window halts the CPU.

Run with `-n` (`--headless`) to never open the window. `VID_FLIP` still counts
frames but doesn't wait, so the program runs as fast as it can. The emulator
uses SDL2 for the window. If CMake can't find SDL2, it builds without the
window and every run behaves as if `-n` was given.

## Example programs

| Program                | Shows |
|------------------------|-------|
| `programs/hello.asm`   | Printing a string with `TTY_OUT`. |
| `programs/greet.asm`   | Reading a line with `TTY_STAT` and `TTY_IN`. |
| `programs/pong.asm`    | Two-player Pong using the framebuffer, `KEYS`, `VID_FLIP` and `TICKS`. |

```text
python3 tools/m16asm.py programs/pong.asm pong.bin
./build/minnie pong.bin
```

In Pong, the left paddle moves with W and S and the right one with Up and
Down. Space serves the ball and Escape quits. The score is printed to the
terminal after every point.
