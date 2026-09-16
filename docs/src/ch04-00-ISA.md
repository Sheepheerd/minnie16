# Instruction Set Architecture

Minnie16 is a 16-bit load/store machine. Every instruction is one 16-bit word,
the top four bits are always the opcode, and there are exactly sixteen opcodes.

This chapter describes the instruction set as implemented by the reference
emulator in `src/` (`decoder.c` and `cpu.c`). Where the emulator does something
surprising, it is called out in [Emulator notes](#emulator-notes) at the end.

## Machine state

| State           | Width   | Notes                                                    |
|-----------------|---------|----------------------------------------------------------|
| `r0`            | 16 bits | Always reads as zero. Writes are discarded.              |
| `r1`–`r15`      | 16 bits | General purpose. No calling convention is defined yet.   |
| `pc`            | 16 bits | Program counter. Not addressable as a register. Resets to `0x0000`. |
| Halt flag       | 1 bit   | Set by `HALT` or by a jump to itself.                    |

There is no flags register. Comparisons produce a value in a register (`SLT`)
and branches test a register against zero (`BEQZ`, `BNEZ`).

## Memory

- 64 KiB address space (`0x0000`–`0xFFFF`), byte addressable.
- Words are **little endian**: the low byte is stored at `addr`, the high byte at `addr + 1`.
- Instructions are word aligned. The `pc` advances by 2 after each instruction.
- Program images are loaded as raw bytes starting at address `0x0000`, and
  execution starts there.

There is no memory-mapped I/O yet.

## Instruction formats

All formats share the opcode and `rd` fields. The low byte is interpreted per format.

```text
          15    12 11     8 7      4 3      0
         +--------+--------+--------+--------+
  R-type | opcode |   rd   |  rs1   |  rs2   |
         +--------+--------+--------+--------+
  I-type | opcode |   rd   |   rb   |  imm4  |
         +--------+--------+--------+--------+
  L-type | opcode |   rd   |       imm8      |
         +--------+--------+-----------------+
  B-type | opcode |   rd   |      offset     |
         +--------+--------+-----------------+
```

| Field    | Bits    | Meaning                                                         |
|----------|---------|-----------------------------------------------------------------|
| `opcode` | `15:12` | Selects the instruction.                                        |
| `rd`     | `11:8`  | Destination register. For `SW`, `BEQZ` and `BNEZ` it is a source. |
| `rs1`    | `7:4`   | First source register.                                          |
| `rs2`    | `3:0`   | Second source register.                                         |
| `rb`     | `7:4`   | Base register for loads, stores and `JALR`.                     |
| `imm4`   | `3:0`   | 4-bit immediate. Sign extended for `ADDI`, zero extended for `LW` and `SW`. |
| `imm8`   | `7:0`   | 8-bit immediate. Sign handling depends on the instruction.      |
| `offset` | `7:0`   | 8-bit **signed** offset (−128 to 127).                          |

## Opcode map

| Opcode | Mnemonic | Format | Syntax                | Semantics                                         |
|:------:|----------|:------:|-----------------------|---------------------------------------------------|
| `0x0`  | `ADD`    | R      | `ADD rd, rs1, rs2`    | `rd ← rs1 + rs2`                                  |
| `0x1`  | `SUB`    | R      | `SUB rd, rs1, rs2`    | `rd ← rs1 − rs2`                                  |
| `0x2`  | `ADDI`   | I      | `ADDI rd, rb, imm4`   | `rd ← rb + sext(imm4)`                            |
| `0x3`  | `AND`    | R      | `AND rd, rs1, rs2`    | `rd ← rs1 & rs2`                                  |
| `0x4`  | `OR`     | R      | `OR rd, rs1, rs2`     | <code>rd ← rs1 &#124; rs2</code>                        |
| `0x5`  | `SLL`    | R      | `SLL rd, rs1, rs2`    | `rd ← rs1 << rs2`, or 0 if `rs2 ≥ 16`             |
| `0x6`  | `SLT`    | R      | `SLT rd, rs1, rs2`    | `rd ← (rs1 < rs2) ? 1 : 0`, unsigned compare      |
| `0x7`  | `LW`     | I      | `LW rd, imm4(rb)`     | `rd ← MEM16[rb + zext(imm4)]`                     |
| `0x8`  | `SW`     | I      | `SW rd, imm4(rb)`     | `MEM16[rb + zext(imm4)] ← rd`                     |
| `0x9`  | `LIL`    | L      | `LIL rd, imm8`        | `rd ← zext(imm8)`                                 |
| `0xA`  | `LIH`    | L      | `LIH rd, imm8`        | <code>rd ← (imm8 &lt;&lt; 8) &#124; (rd &amp; 0x00FF)</code> |
| `0xB`  | `BEQZ`   | B      | `BEQZ rd, offset`     | if `rd == 0`: `pc ← pc + 2 + offset × 2`          |
| `0xC`  | `BNEZ`   | B      | `BNEZ rd, offset`     | if `rd != 0`: `pc ← pc + 2 + offset × 2`          |
| `0xD`  | `JALR`   | I      | `JALR rd, rb`         | `rd ← pc + 2`, `pc ← rb & 0xFFFE`                 |
| `0xE`  | `JMP`    | B      | `JMP offset`          | `pc ← pc + 2 + offset × 2`                        |
| `0xF`  | `HALT`   | R      | `HALT`                | Stop execution.                                   |

In the table, `pc` is the address of the instruction being executed.
`MEM16[a]` is the little-endian word at `a` and `a + 1`. All arithmetic wraps
modulo 2<sup>16</sup>. `zext` means zero extend to 16 bits and `sext` means
sign extend to 16 bits.

The syntax column is the notation used in this book. There is no assembler
yet, so programs are currently built by hand (see [Example program](#example-program)).

### Arithmetic and logic

`ADD`, `SUB`, `AND`, `OR`, `SLL` and `SLT` take two registers and write the
result to `rd`. There are no overflow or carry flags.

`SLT` compares both operands as unsigned 16-bit values. There is no signed
compare.

`ADDI` is the only instruction with an immediate operand for arithmetic. Its
immediate is 4 bits and sign extended, so it adds a value from −8 to 7. For
example, `0x210F` is `ADDI r1, r0, -1` and sets `r1` to `0xFFFF`. For larger
constants, load the constant into a register and use `ADD` or `SUB`.

`SLL` uses the full 16-bit value of `rs2` as the shift amount. Amounts of 16
or more give 0.

There is no `XOR`, `NOT`, right shift, multiply or divide instruction.

### Loading constants

`LIL` (load immediate low) sets a register from an 8-bit value. The value is
zero extended, so `LIL r1, 0x7F` gives `0x007F` and `LIL r1, 0x80` gives
`0x0080`.

`LIH` (load immediate high) replaces the upper byte of a register and keeps the
lower byte.

To load any 16-bit constant, use `LIL` first and then `LIH`. The order matters:
`LIL` clears the upper byte.

```text
LIL r4, 0x12     ; r4 = 0x0012
LIH r4, 0x34     ; r4 = 0x3412
```

For constants from −8 to 7, `ADDI rd, r0, imm4` also works.

### Memory access

`LW` and `SW` address memory as `rb + imm4`, where `imm4` is an unsigned
offset from 0 to 15 bytes. Both read or write a full 16-bit word, so a word
occupies two addresses. Keep offsets even when you work with arrays of words.

For `SW`, the value stored comes from the register in the `rd` field.

### Control flow

`BEQZ` and `BNEZ` test the register in the `rd` field against zero. `JMP` is
unconditional and ignores `rd`. For all three, the offset counts
**instructions**, relative to the next instruction. The range is −128 to +127
instructions.

`JALR` jumps to the address in `rb` and saves a return address in `rd`. The low
bit of the target is cleared, so the target is always word aligned. The `imm4`
field is ignored. The saved return address is the next instruction (`pc + 2`).
To return from a subroutine, use `JALR r0, <link register>`, which discards
the new link.

### Halting

The emulator stops when it executes `HALT`, or when an instruction leaves the
`pc` unchanged (for example `JMP -1`). The encoding `0xF000` is the canonical
`HALT`.

## Example program

`programs/main.py` builds `program.bin`, which stores the value 10 at address
`0x0040`:

| Address  | Word     | Bytes on disk | Instruction          |
|----------|----------|---------------|----------------------|
| `0x0000` | `0x210A` | `0A 21`       | `ADDI r1, r0, 10`    |
| `0x0002` | `0x9240` | `40 92`       | `LIL r2, 0x40`       |
| `0x0004` | `0x8120` | `20 81`       | `SW r1, 0(r2)`       |
| `0x0006` | `0xF000` | `00 F0`       | `HALT`               |

Each word is built by shifting the fields into place and is written with
`struct.pack("<H", word)`, which produces the little-endian byte order.

```python
(OP_ADDI << 12) | (1 << 8) | (0 << 4) | 10   # ADDI r1, r0, 10
```

## Emulator notes

The reference emulator has some behavior that is probably not intended. It is
listed here so that programs do not depend on it by accident. Each item should
be resolved either by fixing the emulator or by adopting it into the
specification above.

- **Reading or writing `0xFFFF`.** A word access at `0xFFFF` touches byte
  `0x10000`, which is outside the 64 KiB `ram` array.
