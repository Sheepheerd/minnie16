# Assembler

`tools/m16asm.py` turns a text file of Minnie16 assembly into a `program.bin`
that the emulator can run. It is a small Python script with no dependencies.

```text
python3 tools/m16asm.py programs/example.asm            # writes program.bin
python3 tools/m16asm.py programs/example.asm out.bin    # pick the output name
```

The emulator loads `program.bin` from the directory it is started in.

## Source format

One instruction per line. Everything after `;` is a comment. A line can start
with a label followed by `:`.

```text
; sums 1..10 and stores the result (55 = 0x37) at 0x0100

        LI   r1, 10        ; n
        MOV  r2, r0        ; sum = 0
        CALL sum
        LI   r3, 0x0100
        SW   r2, 0(r3)
        JMP  done
        NOP                ; skipped
done:   HALT

sum:    ADD  r2, r2, r1
        ADDI r1, r1, -1
        BNEZ r1, sum
        RET
```

- Mnemonics are not case sensitive. Labels are.
- Registers are `r0` to `r15`.
- Numbers can be decimal (`10`, `-1`), hex (`0x40`) or binary (`0b1010`).
- Commas between operands are optional. The assembler treats them as spaces.

## Instructions

Every opcode from the [Instruction Set Architecture](./ch04-00-ISA.md) chapter
is supported, written the same way as in that chapter.

| Format | Syntax | Notes |
|--------|--------|-------|
| R | `ADD rd, rs1, rs2` | Also `SUB`, `AND`, `OR`, `SLL`, `SLT`. |
| I | `ADDI rd, rb, imm` | `imm` must be −8 to 7. |
| I | `LW rd, off(rb)` | Also `SW`. `off` is in bytes and is divided by 2. `(rb)` means an offset of 0. |
| L | `LIL rd, imm` | Also `LIH`. Only the low 8 bits of `imm` are kept. |
| B | `BEQZ rd, target` | Also `BNEZ`. |
| I | `JALR rd, rb` | |
| J | `JAL target` | |
| X | `XOR rd, rs` | Also `SRL`, `SRA`, `SLTS`. |
| X | `LB rd, (rs)` | Also `SB`. |
| X | `HALT` | No operands. |

### Branch targets

A branch or `JAL` target is either a label or a number:

- **Label:** the assembler works out the offset, counted in instructions from
  the next instruction.
- **Number:** used as the raw offset. `BEQZ r0, -1` branches to itself.

```text
loop:   BNEZ r1, loop      ; offset -1, encodes as 0xC1FF
        BEQZ r0, -1        ; same thing, written by hand
```

## Pseudo-instructions

These are shorthand for real instructions. The assembler replaces them before
it assigns addresses, so labels after them still land in the right place.

| Pseudo | Becomes | Words |
|--------|---------|:-----:|
| `NOP` | `ADD r0, r0, r0` | 1 |
| `MOV rd, rs` | `ADD rd, rs, r0` | 1 |
| `JMP target` | `BEQZ r0, target` | 1 |
| `CALL target` | `JAL target` | 1 |
| `RET` | `JALR r0, r15` | 1 |
| `LI rd, imm16` | `LIL rd, low byte` then `LIH rd, high byte` | 2 |

`LI` always takes two words, even for small values. It only takes a number,
not a label.

`CALL` uses `JAL`, so the return address goes in `r15`. A subroutine that
calls another subroutine must save `r15` first.

## How it works

The assembler makes two passes over the file.

1. **Parse.** For each line: drop the comment, record the label at the current
   address, split the rest into a mnemonic and operands, and expand any
   pseudo-instruction. Every real instruction gets the next address, 2 bytes
   apart.
2. **Encode.** For each instruction, look up its opcode and format in the
   `OPS` table, then shift the operand fields into place. All labels are known
   by now, so forward branches work.

The words are written to the output file in little-endian order, the same as
`struct.pack("<H", word)`.

## Errors

The assembler stops at the first error and prints the line number:

```text
line 3: error: unknown instruction FOO
line 7: error: ADDI immediate must be -8..7
line 9: error: no register r16
```

## Limitations

The assembler is meant to stay small. It does not check:

- That `LW`/`SW` offsets are even and from 0 to 30.
- That `LIL`/`LIH` values fit in 8 bits.
- That branch targets are in range (−128 to 127 for `BEQZ`/`BNEZ`, −2048 to 2047
  for `JAL`).
- Duplicate or undefined labels. An unknown label name is parsed as a number,
  which fails with a Python `ValueError` message.

Out-of-range values are masked to fit their field, so they assemble without an
error but run the wrong instruction.

There are no directives yet, so a program cannot contain data like `.word` or
strings. Everything is code, starting at address `0x0000`.
