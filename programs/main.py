import struct

OP_ADD   = 0x0
OP_SUB   = 0x1
OP_ADDI  = 0x2
OP_AND   = 0x3
OP_OR    = 0x4
OP_SLL   = 0x5
OP_SLT   = 0x6
OP_LW    = 0x7
OP_SW    = 0x8
OP_LIL   = 0x9
OP_LIH   = 0xA
OP_BEQZ  = 0xB
OP_BNEZ  = 0xC
OP_JALR  = 0xD
OP_JMP   = 0xE
OP_HALT  = 0xF

instructions = [
    # r1 = 0 + 10
    (OP_ADDI << 12) | (1 << 8) | (0 << 4) | 10,

    (OP_LIL << 12) | (2 << 8) | 0x40,

    # mem[reg[2] + 0] = r1
    (OP_SW << 12) | (1 << 8) | (2 << 4) | 0,

    # halt
    (OP_HALT << 12),
]

with open("program.bin", "wb") as f:
    for inst in instructions:
        f.write(struct.pack("<H", inst))

print("Wrote program.bin successfully.")
