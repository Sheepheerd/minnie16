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
| `r1`–`r14`      | 16 bits | General purpose. No calling convention is defined yet.   |
| `r15`           | 16 bits | General purpose. `JAL` writes its return address here.   |
| `pc`            | 16 bits | Program counter. Not addressable as a register. Resets to `0x0000`. |
| Halt flag       | 1 bit   | Set by `HALT` or by a jump to itself.                    |

There is no flags register. Comparisons produce a value in a register (`SLT`, `SLTS`)
and branches test a register against zero (`BEQZ`, `BNEZ`).

## Memory

- 64 KiB address space (`0x0000`–`0xFFFF`), byte addressable.
- Words are **little endian**: the low byte is stored at `addr`, the high byte at `addr + 1`.
- Instructions are word aligned. The `pc` advances by 2 after each instruction.
- Program images are loaded as raw bytes starting at address `0x0000`, and
  execution starts there.

There is no memory-mapped I/O yet.

## Instruction formats

Every format starts with the 4-bit opcode. All formats except J-type also have
an `rd` field, and the remaining bits are interpreted per format.

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
  J-type | opcode |         offset12         |
         +--------+--------+-----------------+
  X-type | opcode |   rd   |   rs   | funct  |
         +--------+--------+--------+--------+
```

| Field    | Bits    | Meaning                                                         |
|----------|---------|-----------------------------------------------------------------|
| `opcode` | `15:12` | Selects the instruction.                                        |
| `rd`     | `11:8`  | Destination register. For `SW`, `BEQZ` and `BNEZ` it is a source. |
| `rs1`    | `7:4`   | First source register.                                          |
| `rs2`    | `3:0`   | Second source register.                                         |
| `rb`     | `7:4`   | Base register for loads, stores and `JALR`.                     |
| `imm4`   | `3:0`   | 4-bit immediate. Signed value for `ADDI`, unsigned word offset for `LW` and `SW`. |
| `imm8`   | `7:0`   | 8-bit immediate. Sign handling depends on the instruction.      |
| `offset` | `7:0`   | 8-bit **signed** offset (−128 to 127).                          |
| `offset12` | `11:0` | 12-bit **signed** offset (−2048 to 2047).                     |
| `rs`     | `7:4`   | Source register or address for `EXT` operations.                |
| `funct`  | `3:0`   | Selects the `EXT` operation.                                    |

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
| `0x7`  | `LW`     | I      | `LW rd, off(rb)`      | `rd ← MEM16[rb + zext(imm4) × 2]`                 |
| `0x8`  | `SW`     | I      | `SW rd, off(rb)`      | `MEM16[rb + zext(imm4) × 2] ← rd`                 |
| `0x9`  | `LIL`    | L      | `LIL rd, imm8`        | `rd ← zext(imm8)`                                 |
| `0xA`  | `LIH`    | L      | `LIH rd, imm8`        | <code>rd ← (imm8 &lt;&lt; 8) &#124; (rd &amp; 0x00FF)</code> |
| `0xB`  | `BEQZ`   | B      | `BEQZ rd, offset`     | if `rd == 0`: `pc ← pc + 2 + offset × 2`          |
| `0xC`  | `BNEZ`   | B      | `BNEZ rd, offset`     | if `rd != 0`: `pc ← pc + 2 + offset × 2`          |
| `0xD`  | `JALR`   | I      | `JALR rd, rb`         | `rd ← pc + 2`, `pc ← rb & 0xFFFE`                 |
| `0xE`  | `JAL`    | J      | `JAL offset12`        | `r15 ← pc + 2`, `pc ← pc + 2 + offset12 × 2`      |
| `0xF`  | `EXT`    | X      | see below             | Extended operations, selected by `funct`.         |

### `EXT` operations

Opcode `0xF` holds operations that take two registers. Most of them write the
result back to `rd`. `LB` and `SB` use `rs` as a byte address.

| `funct` | Mnemonic | Syntax          | Semantics                                              |
|:-------:|----------|-----------------|--------------------------------------------------------|
| `0x0`   | `HALT`   | `HALT`          | Stop execution. `rd` and `rs` are ignored.             |
| `0x1`   | `XOR`    | `XOR rd, rs`    | `rd ← rd ^ rs`                                         |
| `0x2`   | `SRL`    | `SRL rd, rs`    | `rd ← rd >> rs`, logical. 0 if `rs ≥ 16`.              |
| `0x3`   | `SRA`    | `SRA rd, rs`    | `rd ← rd >> rs`, arithmetic. Amounts of 16 or more act as 15. |
| `0x4`   | `SLTS`   | `SLTS rd, rs`   | `rd ← (rd < rs) ? 1 : 0`, signed compare               |
| `0x5`   | `LB`     | `LB rd, (rs)`   | `rd ← zext(MEM8[rs])`                                  |
| `0x6`   | `SB`     | `SB rd, (rs)`   | `MEM8[rs] ← rd & 0xFF`                                 |
| `0x7`–`0xF` | —    | —               | Reserved. The emulator halts with an error.            |

Because `EXT` operations overwrite `rd`, copy a value first if you still need
it:

```text
ADD  r3, r1, r0   ; r3 = r1
SRL  r3, r2       ; r3 = r1 >> r2, r1 unchanged
```

In the table, `pc` is the address of the instruction being executed.
`MEM16[a]` is the little-endian word at `a` and `a + 1`, and `MEM8[a]` is the
byte at `a`. All arithmetic wraps
modulo 2<sup>16</sup>. `zext` means zero extend to 16 bits and `sext` means
sign extend to 16 bits.

The syntax column is the notation used in this book. There is no assembler
yet, so programs are currently built by hand (see [Example program](#example-program)).

### Arithmetic and logic

`ADD`, `SUB`, `AND`, `OR`, `SLL` and `SLT` take two registers and write the
result to `rd`. There are no overflow or carry flags.

`SLT` compares both operands as unsigned 16-bit values. `SLTS` is the signed
version.

`ADDI` is the only instruction with an immediate operand for arithmetic. Its
immediate is 4 bits and sign extended, so it adds a value from −8 to 7. For
example, `0x210F` is `ADDI r1, r0, -1` and sets `r1` to `0xFFFF`. For larger
constants, load the constant into a register and use `ADD` or `SUB`.

`SLL` uses the full 16-bit value of `rs2` as the shift amount. Amounts of 16
or more give 0.

`XOR`, `SRL`, `SRA` and `SLTS` are in the [`EXT` group](#ext-operations).
There is no `NOT`, multiply or divide instruction. To get `NOT`, `XOR` with
`0xFFFF`.

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

Memory is byte addressable. There are two pairs of load and store instructions:
`LW`/`SW` move a 16-bit word, and `LB`/`SB` move a single byte.

`LW` and `SW` address memory as `rb + imm4 × 2`. The offset counts **words**,
so it reaches 16 word slots, 0 to 30 bytes above `rb`. In this book's syntax,
the offset is written in bytes and must be even. The encoding stores the offset
divided by 2:

```text
SW r1, 6(r2)     ; imm4 = 3, MEM16[r2 + 6] ← r1
LW r3, 30(r2)    ; imm4 = 15, r3 ← MEM16[r2 + 30]
```

The base register can still hold any address, odd or even. Only the offset is
limited to even values.

`LB` and `SB` are in the [`EXT` group](#ext-operations) and have no offset. The
address is the value of `rs`. `LB` zero extends the byte into `rd`, and `SB`
stores the low byte of `rd`. Use them for strings, byte buffers and byte-wide
device registers:

```text
LIL  r2, 0x40    ; r2 = address of a string
LB   r1, (r2)    ; r1 = first character
ADDI r2, r2, 1   ; next byte
LB   r3, (r2)    ; r3 = second character
```

For `SW` and `SB`, the value stored comes from the register in the `rd` field.

### Control flow

`BEQZ` and `BNEZ` test the register in the `rd` field against zero. The offset
counts **instructions**, relative to the next instruction. The range is −128 to
+127 instructions.

There is no separate unconditional jump. `r0` is always zero, so
`BEQZ r0, offset` always branches. An assembler can provide `JMP offset` as an
alias for it.

`JAL` calls a subroutine within ±2048 instructions. It saves the address of the
next instruction in `r15` and branches by `offset12` instructions. For targets
outside that range, load the address with `LIL` and `LIH` and use `JALR`.

`JALR` jumps to the address in `rb` and saves a return address in `rd`. The low
bit of the target is cleared, so the target is always word aligned. The `imm4`
field is ignored. The saved return address is the next instruction (`pc + 2`).
To return from a subroutine, use `JALR r0, r15` (or whichever register holds
the link), which discards the new link.

```text
      JAL  func        ; r15 = return address
      HALT
func: ...
      JALR r0, r15     ; return
```

A subroutine that calls another subroutine must save `r15` first.

### Halting

The emulator stops when it executes `HALT`, or when an instruction leaves the
`pc` unchanged (for example `BEQZ r0, -1` or `JAL -1`). The encoding `0xF000` is the canonical
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
