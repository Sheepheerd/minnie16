---
title: Minnie16 Architecture Choices
sub_title: What the instruction set gives up, and what it buys
author: Shepard Garrett
---

The one constraint
===

Every instruction is **one 16-bit word**. Nothing is longer.

<!-- pause -->

That single decision sets the whole budget:

- 4 bits of opcode leaves 12 bits for operands.
- 4 bits of opcode means **exactly 16 opcodes**. Not 15, not 17.
- 4 bits per register field means **16 registers**, `r0`–`r15`.

<!-- pause -->

Nothing else in this chapter is free to change independently. Every later
choice is spending from these 16 bits.

<!-- end_slide -->

Load/store, and a zero register
===

<!-- column_layout: [1, 1] -->

<!-- column: 0 -->

## The rule

Arithmetic touches registers only. Memory moves through `LW`, `SW`, `LB`, `SB`
and nothing else.

Decode stays uniform: no instruction both computes and addresses memory.

<!-- column: 1 -->

## The trick

`r0` always reads as zero, and writes to it are discarded.

One hardwired register buys three idioms for free:

```text
ADD  r3, r1, r0   ; move
ADDI r1, r0, -1   ; small constant
BEQZ r0, offset   ; unconditional jump
```

<!-- reset_layout -->

<!-- pause -->

**There is no jump opcode**.

<!-- end_slide -->

No flags register
===

Comparison writes a value. Branching reads a register.

| Instead of | Minnie16 does |
|------------|---------------|
| `CMP` then `BLT` | `SLT rd, rs1, rs2` then `BNEZ rd, offset` |
| Carry out of `ADD` | nothing — arithmetic wraps mod 2^16 |
| Overflow trap | nothing |

<!-- pause -->

**What it buys:** no hidden state between instructions :).

**What it costs:** every comparison burns a register and an extra instruction.
Signed and unsigned need separate opcodes, `SLT` and `SLTS`.

<!-- end_slide -->

Six formats, one field layout
===

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

<!-- pause -->

`opcode` is always `15:12`. `rd` is always `11:8` in five formats out of six.
A decoder can pull both fields before it knows which instruction it is holding.

<!-- end_slide -->

The immediate problem
===

An I-type immediate is **4 bits**. Signed, that is −8 to 7.

<!-- pause -->

So constants get their own format. `LIL` and `LIH` each carry 8 bits, and the
pair covers the full 16-bit range:

```text
LIL r4, 0x12     ; r4 = 0x0012   (zero extends, clears the high byte)
LIH r4, 0x34     ; r4 = 0x3412   (replaces high byte, keeps low byte)
```

<!-- pause -->

Order matters, and the asymmetry is deliberate: `LIL` clears, `LIH` merges.

<!-- end_slide -->

Scaled offsets
===

Memory is byte addressable, but `LW` and `SW` scale their offset by 2:

```text
MEM16[rb + zext(imm4) × 2]
```

<!-- pause -->

<!-- column_layout: [1, 1] -->

<!-- column: 0 -->

## Without scaling

4 bits reach 15 bytes above `rb`.

Seven and a half words. Half of them misaligned.

<!-- column: 1 -->

## With scaling

4 bits reach 30 bytes above `rb`.

Sixteen words. All aligned, by construction.

<!-- reset_layout -->

<!-- pause -->

Alignment is not checked, it doesn't need to be. `LB` and `SB` keep the raw
byte addressing for strings and byte-wide registers, and pay for it with no
offset field at all.

<!-- end_slide -->

EXT: the sixteenth opcode
===

Fifteen opcodes are real instructions. `0xF` is an "escape hatch".

```text
  X-type | opcode |   rd   |   rs   | funct  |
```

`funct` selects: `HALT`, `XOR`, `SRL`, `SRA`, `SLTS`, `LB`, `SB`, and nine
reserved slots.

<!-- pause -->

The cost shows up in the operand count. X-type spends 4 bits on `funct`, so
only two register fields are left — these operations are **destructive**:

```text
ADD  r3, r1, r0   ; copy first
SRL  r3, r2       ; r3 = r1 >> r2, r1 survives
```

<!-- pause -->

`ADD` gets a three-operand form; `XOR` does not.

<!-- end_slide -->

Branch ranges
===

| Instruction | Offset field | Counts | Reach |
|-------------|--------------|--------|-------|
| `BEQZ`, `BNEZ` | 8-bit signed | instructions | −128 to +127 |
| `JAL` | 12-bit signed | instructions | −2048 to +2047 |
| `JALR` | none | absolute, from `rb` | anywhere |

<!-- pause -->

Offsets count **instructions** (words), not bytes — the same ×2 trick as `LW`, doubling
reach and making a misaligned branch target impossible to encode.

<!-- pause -->

`JAL` drops `rd` entirely to afford 12 bits, so the link register is fixed at
`r15`. `JALR` gets it back and clears the low bit of the target, which is how
out-of-range calls are made:

```text
LIL  r5, lo ; LIH r5, hi
JALR r15, r5
```

Nesting calls means saving `r15` yourself. There is no stack pointer and no
calling convention yet.

<!-- end_slide -->

What was left out
===

No multiply. No divide. No `NOT`. No stack. No interrupts. No MMIO, so far.

<!-- pause -->

Each absence has a workaround, and the workaround is the argument:

- `NOT` is `XOR` with `0xFFFF`.
- A stack is `ADDI` on a register you chose to call a stack pointer.
- Multiply is a loop over `SLL` and `ADD`.

<!-- pause -->

An instruction earns its opcode by being hard to synthesize, not by being
useful. Since this is a sim, it didn't matter, but it's about playing close to the heart.


<!-- end_slide -->

Summary
===

The 16-bit instruction word is the only real constraint. Everything else is
what that constraint forced:

| Choice | Bought |
|--------|--------|
| `r0` hardwired to zero | move, small constants, unconditional jump |
| No flags | no hidden state between instructions |
| `LIL` + `LIH` | full 16-bit constants from 8-bit fields |
| Offsets scaled by 2 | double the reach, alignment by construction |
| `EXT` escape hatch | seven more operations, at two operands each |

