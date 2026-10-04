---
title: Minnie16 Assembler
sub_title: From text to 16-bit words
author: Shepard Garrett
---

Where we left off
===

The CPU works, but every program was built by hand:

```python
(OP_ADDI << 12) | (1 << 8) | (0 << 4) | 10   # ADDI r1, r0, 10
```

<!-- pause -->

That line has a bug. `imm4` is signed, so `10` is really **−6**.

Nobody noticed, because nothing was checking. An assembler is the thing that
checks.

<!-- end_slide -->

The goal
===

Turn this:

```text
loop:   ADD  r2, r2, r1
        ADDI r1, r1, -1
        BNEZ r1, loop
```

<!-- pause -->

Into this:

```text
0x0221  0x211F  0xC1FD
```

<!-- pause -->

One Python file. No dependencies. Around 150 lines.

```bash
python3 tools/m16asm.py programs/example.asm
```

<!-- end_slide -->

One table drives everything
===

Sixteen opcodes, but only a handful of operand shapes:

```python
OPS = {
    "ADD":  (0x0, "R", 0),
    "ADDI": (0x2, "I", 0),
    "LW":   (0x7, "M", 0),
    "LIL":  (0x9, "L", 0),
    "BEQZ": (0xB, "B", 0),
    "JAL":  (0xE, "J", 0),
    "XOR":  (0xF, "X", 0x1),
    ...
}
```

<!-- pause -->

Adding an instruction is one line, as long as it fits a format we already have.
That is the six formats from the last talk paying off.

<!-- end_slide -->

Two passes
===

<!-- column_layout: [1, 1] -->

<!-- column: 0 -->

## Pass 1: parse

- Drop `;` comments
- Record `label:` at current address
- Split mnemonic and operands
- Expand pseudo-instructions
- Advance address by 2 per instruction

<!-- column: 1 -->

## Pass 2: encode

- Look up opcode and format
- Shift each field into place
- Labels are all known now
- Write words little endian

<!-- reset_layout -->

<!-- pause -->

Why two? **Forward branches.** `JMP done` comes before `done:`, so the first
pass has to find every label before anything can be encoded.

<!-- end_slide -->

Encoding is just shifting
===

Same layout as the format diagram, written as Python:

```python
if fmt == "R":    # ADD rd, rs1, rs2
    word |= reg(args[0]) << 8 | reg(args[1]) << 4 | reg(args[2])
elif fmt == "L":  # LIL rd, imm8
    word |= reg(args[0]) << 8 | (int(args[1], 0) & 0xFF)
elif fmt == "J":  # JAL label
    word |= offset(args[0], labels, pc) & 0xFFF
```

<!-- pause -->

The only real math is in two places:

| Where | Rule |
|-------|------|
| `LW`/`SW` offset | written in bytes, encoded as `off / 2` |
| Branch to label | `(target - (pc + 2)) / 2` instructions |

<!-- end_slide -->

Pseudo-instructions
===

The ISA left things out on purpose. The assembler puts them back as shorthand:

| Write | Get |
|-------|-----|
| `NOP` | `ADD r0, r0, r0` |
| `MOV rd, rs` | `ADD rd, rs, r0` |
| `JMP label` | `BEQZ r0, label` |
| `CALL label` | `JAL label` |
| `RET` | `JALR r0, r15` |
| `LI rd, imm16` | `LIL` + `LIH` |

<!-- pause -->

Almost all of them lean on `r0` being zero. That register keeps paying for
itself.

<!-- pause -->

They are expanded in **pass 1**, before addresses are handed out. `LI` is two
words, so every label after it shifts by 2, and the branches still land.

<!-- end_slide -->

It runs
===

```text
        LI   r1, 10        ; n
        MOV  r2, r0        ; sum = 0
        CALL sum
        LI   r3, 0x0100
        SW   r2, 0(r3) -----------> Base + Displacement Addressing (MIPS + RISC-V)
        HALT
sum:    ADD  r2, r2, r1
        ADDI r1, r1, -1
        BNEZ r1, sum
        RET
```

<!-- pause -->

```text
Enter memory address to view (hex): 100
Memory at 0x0100: 0x37
```

`0x37` is 55. Sum of 1 to 10.

<!-- end_slide -->

Fibonacci
===

<!-- column_layout: [3, 2] -->

<!-- column: 0 -->

```text
        LI   r1, 0x0100    ; where to put them
        LI   r4, 10        ; how many
        CALL fib
        HALT

; fib: r1 = destination, r4 = count
fib:    MOV  r2, r0        ; a = 0
        ADDI r3, r0, 1     ; b = 1
loop:   SW   r2, 0(r1)     ; store a
        ADD  r5, r2, r3    ; next = a + b
        MOV  r2, r3        ; a = b
        MOV  r3, r5        ; b = next
        ADDI r1, r1, 2     ; next word
        ADDI r4, r4, -1
        BNEZ r4, loop
        RET
```

<!-- column: 1 -->

Press `ctrl+e` to assemble and run it:

```bash +exec
cd "$(git rev-parse --show-toplevel)"
python3 tools/m16asm.py programs/fib.asm build/program.bin
cd build
printf "%x\n" $(seq 256 2 274) | ./minnie | grep -m 10 Memory |
  while read -r line; do echo "fib[$((i++))] = $((${line##* }))"; done
```

<!-- reset_layout -->

<!-- end_slide -->

Kept simple on purpose
===

What it checks:

- Unknown instruction
- Register past `r15`
- `ADDI` immediate outside −8 to 7

<!-- pause -->

What it does not, yet:

- `LW`/`SW` offsets, `LIL` values, branch ranges
- Duplicate or missing labels
- Data. No `.word`, no strings. Everything is code at `0x0000`.

<!-- pause -->

Small enough to read in one sitting. Each missing piece is a few lines when it
is actually needed.

<!-- end_slide -->

What's Next
===

Writing programs is easy now. Watching them run is not: the emulator prints a
wall of `Decoded:` lines and then asks for memory addresses.

<!-- pause -->

Next up on the roadmap: a **step-by-step debugger**.
