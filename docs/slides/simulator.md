---
title: Minnie16 Simulator
sub_title: Assembling a program, then watching it run
author: Shepard Garrett
---

Where we left off
===

The assembler works. Running a program printed this:

```text
Decoded: op=9, rd=1, imm=0
Decoded: op=10, rd=1, imm=1
Decoded: op=14, offset=1
...
```

<!-- pause -->

Every instruction, all at once. You had to already know that `op=14` is `JAL`.

<!-- pause -->

To watch a program run, you need to be able to **stop it**.

<!-- end_slide -->

The pipeline
===

```text
  fib.asm ──► m16asm.py ──► fib.bin ──► minnie
   text        Python       raw words      C
```

<!-- pause -->

<!-- column_layout: [1, 1] -->

<!-- column: 0 -->

## Assembler

Text in, little-endian words out. Two passes, so forward labels work.

<!-- column: 1 -->

## Simulator

Loads raw bytes at `0x0000`. Runs to `HALT`, or stops and waits for you.

<!-- reset_layout -->

<!-- pause -->

The only contract between them is the binary format: 16-bit words, low byte
first, starting at address zero.

<!-- end_slide -->

The assembler, recapped
===

One table, two passes:

```python
OPS = {
    "ADD":  (0x0, "R", 0),
    "LW":   (0x7, "M", 0),
    "BEQZ": (0xB, "B", 0),
    "XOR":  (0xF, "X", 0x1),
    ...
}
```

<!-- pause -->

Plus the pseudo-instructions, which are almost all `r0` tricks:

| Write | Get |
|-------|-----|
| `MOV rd, rs` | `ADD rd, rs, r0` |
| `JMP label` | `BEQZ r0, label` |
| `RET` | `JALR r0, r15` |

Keep this table in mind. The debugger uses it **backwards**.

<!-- end_slide -->

One file, one job
===

| File | Job |
|------|-----|
| `bus.c` | 64 KiB of RAM, byte and word access |
| `decoder.c` | word → `Instruction` |
| `cpu.c` | fetch, decode, execute one instruction |
| `debug.c` | `Instruction` → text |
| `simulator.c` | breakpoints, run loop, commands |
| `main.c` | run mode or debug mode |

<!-- pause -->

The CPU knows nothing about breakpoints. The debugger knows nothing about
opcodes. `simulator.c` sits between them.

<!-- end_slide -->

One step
===

```c
void cpu_step(CPU *cpu, BUS *bus) {
  uint16_t old_pc = cpu->pc;
  Instruction inst = decode(bus_read(bus, cpu->pc));
  cpu->pc += 2;

  switch (inst.op) { ... }

  cpu->reg[0] = 0;          // r0 is hardwired to zero
  if (cpu->pc == old_pc)    // a jump to itself never makes progress
    cpu->halted = 1;
}
```

<!-- pause -->

`r0` is cleared **after** the instruction, not before. Otherwise
`ADD r0, r1, r2` leaves junk in `r0` until the next step, and the debugger
shows it.

<!-- end_slide -->

Two ways to run
===

<!-- column_layout: [1, 1] -->

<!-- column: 0 -->

## Run mode

```bash
minnie fib.bin
```

Runs to `HALT`, prints the registers.

<!-- column: 1 -->

## Debug mode

```bash
minnie -d fib.bin
```

Stops before the first instruction.

<!-- reset_layout -->

<!-- pause -->

Press `ctrl+e` to run `example.asm`, the sum of 1 to 10:

```bash +exec
cd "$(git rev-parse --show-toplevel)"
cmake -B build -S . >/dev/null && cmake --build build >/dev/null
python3 tools/m16asm.py programs/example.asm build/example.bin >/dev/null
build/minnie build/example.bin
```

<!-- end_slide -->

One table, again
===

The debugger is built the same way as the assembler:

```c
static const COMMAND commands[] = {
  {"step",     "s", cmd_step,     "step [N]           execute N instructions"},
  {"continue", "c", cmd_continue, "continue           run to a breakpoint or halt"},
  {"break",    "b", cmd_break,    "break [ADDR]       set a breakpoint, or list them"},
  {"regs",     "r", cmd_regs,     "regs               show registers"},
  {"mem",      "x", cmd_mem,      "mem ADDR [LEN]     dump memory"},
  {"disas",    "l", cmd_disas,    "disas [ADDR] [N]   disassemble N instructions"},
  {"set",   NULL,   cmd_set,      "set REG|ADDR VAL   write a register, pc or word"},
  ...
};
```

<!-- pause -->

Adding a command is one line. `help` just prints the last column.

<!-- pause -->

**Enter** repeats the last command, so `s` then Enter, Enter, Enter walks
through the program one instruction at a time.

<!-- end_slide -->

The assembler, backwards
===

`disassemble()` turns a word back into text, and puts the pseudo-instructions
back:

| Word | Really | Shown as |
|------|--------|----------|
| `0x0000` | `ADD r0, r0, r0` | `NOP` |
| `0x0200` | `ADD r2, r0, r0` | `MOV r2, r0` |
| `0xB0FE` | `BEQZ r0, -2` | `JMP -2 ; -> 0x0000` |
| `0xD0F0` | `JALR r0, r15` | `RET` |

<!-- pause -->

Branch offsets count instructions from the *next* pc, so the disassembler
works out the target address and prints it too.

<!-- pause -->

The output is valid assembler input. A disassembled line can be pasted straight
back into a `.asm` file.

<!-- end_slide -->

Round trip
===

<!-- column_layout: [1, 1] -->

<!-- column: 0 -->

## What I wrote

```text
        LI   r1, 0x0100
        LI   r4, 10
        CALL fib
        HALT

fib:    MOV  r2, r0
        ADDI r3, r0, 1
loop:   SW   r2, 0(r1)
        ADD  r5, r2, r3
        MOV  r2, r3
        MOV  r3, r5
        ADDI r1, r1, 2
        ADDI r4, r4, -1
        BNEZ r4, loop
        RET
```

<!-- column: 1 -->

## What the CPU sees

```bash +exec
cd "$(git rev-parse --show-toplevel)"
cmake -B build -S . >/dev/null && cmake --build build >/dev/null
python3 tools/m16asm.py programs/fib.asm build/fib.bin >/dev/null
printf 'disas 0 16\n' | build/minnie -d build/fib.bin |
  sed -n '4,19p' | cut -c 5- | cut -c 1-46
```

<!-- reset_layout -->

<!-- pause -->

The labels are gone and each `LI` is now two words, but every line still maps
back to the source.

<!-- end_slide -->

Stepping
===

`step` runs one instruction and shows the next one, marked with `=>`.

```bash +exec
cd "$(git rev-parse --show-toplevel)"
cmake -B build -S . >/dev/null && cmake --build build >/dev/null
python3 tools/m16asm.py programs/fib.asm build/fib.bin >/dev/null
printf 'step\nstep\nstep 3\nregs\n' | build/minnie -d build/fib.bin
```

<!-- pause -->

Both `LI`s have run, so `r1 = 0x0100` and `r4 = 10`. `JAL` jumped to `fib` and
left the return address in `r15`.

<!-- end_slide -->

Breakpoints
===

```c
SIM_STATUS sim_step(SIMULATOR *sim) {
  cpu_step(sim->cpu, sim->bus);
  if (sim->cpu->halted)                         return SIM_HALTED;
  if (find_breakpoint(sim, sim->cpu->pc) >= 0)  return SIM_BREAK;
  return SIM_OK;
}
```

`continue` calls this in a loop until it returns something other than
`SIM_OK`.

<!-- pause -->

The check happens **after** the step. Continuing from a breakpoint always runs
at least one instruction, so you never get stuck on the one you're sitting on.

<!-- pause -->

`ctrl+c` breaks out of an infinite loop and drops you back at the prompt.

<!-- end_slide -->

It stops
===

Break on the store at the top of the loop:

```bash +exec
cd "$(git rev-parse --show-toplevel)"
cmake -B build -S . >/dev/null && cmake --build build >/dev/null
python3 tools/m16asm.py programs/fib.asm build/fib.bin >/dev/null
printf 'break 0x10\ncontinue\ncontinue\ndelete\ncontinue\nmem 0x100 20\n' |
  build/minnie -d build/fib.bin
```

<!-- pause -->

`*` marks a breakpoint. Memory is little endian: `0D 00` is 13, `22 00` is 34.

<!-- end_slide -->

Changing state
===

A debugger that can only read is half a debugger.

| Command | Does |
|---------|------|
| `set r4 3` | only compute 3 numbers |
| `set pc 0x14` | jump somewhere else |
| `set 0x100 0xBEEF` | write a word to memory |
| `set r0 5` | refused, `r0` is always zero |
| `reset` | reload the binary, keep breakpoints |

<!-- pause -->

`reset` reads the file from disk again. Re-assemble in another terminal, type
`reset`, and you're debugging the new version with the same breakpoints.

<!-- end_slide -->

Summary
===

The assembler turns text into words. The simulator lets you watch them run.

| Piece | Gives you |
|-------|-----------|
| Run mode | the answer, and every register |
| `step` | one instruction at a time |
| `break` + `continue` | skip to the part you care about |
| Disassembler | the source, rebuilt from memory |
| `set` + `reset` | change things without starting over |

<!-- end_slide -->

What's Next
===

The debugger can see everything, but the program still can't say anything.
The only way to get output is to stop it and read memory.

<!-- pause -->

The next presentation will go over **MMIO**: a console the program can write
to by itself.
