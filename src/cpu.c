#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bus.h"
#include "cpu.h"
#include "decoder.h"

CPU *cpu_init() {
  CPU *cpu = (CPU *)malloc(sizeof(CPU));
  cpu->pc = 0;
  cpu->halted = 0;
  memset(cpu->reg, 0, sizeof(cpu->reg));
  return cpu;
}

void cpu_step(CPU *cpu, BUS *bus, int debug) {
  uint16_t old_pc = cpu->pc;
  uint16_t raw = bus_read(bus, cpu->pc);
  Instruction inst = decode(raw);
  cpu->reg[0] = 0;
  cpu->pc += 2;

  if (inst.fmt == FMT_B) {
    printf("Decoded: op=%d, rd=%d, offset=%d\n", inst.op, inst.rd,
           inst.b.offset);
  } else if (inst.fmt == FMT_I) {
    printf("Decoded: op=%d, rd=%d, rb=%d, imm=%d\n", inst.op, inst.rd,
           inst.i.rb, inst.i.imm);
  } else {
    printf("Decoded: op=%d, rd=%d, rs1=%d, rs2=%d\n", inst.op, inst.rd,
           inst.r.rs1, inst.r.rs2);
  }

  switch (inst.op) {
  case OP_ADD:
    cpu->reg[inst.rd] = cpu->reg[inst.r.rs1] + cpu->reg[inst.r.rs2];
    break;
  case OP_SUB:
    cpu->reg[inst.rd] = cpu->reg[inst.r.rs1] - cpu->reg[inst.r.rs2];
    break;
  case OP_ADDI:
    cpu->reg[inst.rd] =
        cpu->reg[inst.i.rb] + (int8_t)((int8_t)(inst.i.imm << 4) >> 4);
    break;
  case OP_AND:
    cpu->reg[inst.rd] = cpu->reg[inst.r.rs1] & cpu->reg[inst.r.rs2];
    break;
  case OP_OR:
    cpu->reg[inst.rd] = cpu->reg[inst.r.rs1] | cpu->reg[inst.r.rs2];
    break;
  case OP_SLL: {
    uint16_t amt = cpu->reg[inst.r.rs2];
    cpu->reg[inst.rd] = amt >= 16 ? 0 : (uint16_t)(cpu->reg[inst.r.rs1] << amt);
    break;
  }
  case OP_SLT:
    cpu->reg[inst.rd] = cpu->reg[inst.r.rs1] < cpu->reg[inst.r.rs2];
    break;
  case OP_LW: {

    uint16_t base = cpu->reg[inst.i.rb];
    uint16_t offset = inst.i.imm & 0xf;
    cpu->reg[inst.rd] = bus_read(bus, base + offset);
    break;
  }
  case OP_SW: {
    uint16_t base = cpu->reg[inst.i.rb];
    uint16_t offset = inst.i.imm & 0xf;
    uint16_t data = cpu->reg[inst.rd]; // rd holds source data for SW
    bus_write(bus, base + offset, data);
    break;
  }
  case OP_LIL:
    cpu->reg[inst.rd] = inst.l.imm & 0xFF;
    break;
  case OP_LIH:
    cpu->reg[inst.rd] =
        (((uint8_t)inst.l.imm) << 8) | (cpu->reg[inst.rd] & 0x00FF);
    break;
  case OP_BEQZ:
    if (cpu->reg[inst.rd] == 0) {
      cpu->pc += (int8_t)inst.b.offset * 2;
    }
    break;
  case OP_BNEZ:
    if (cpu->reg[inst.rd] != 0) {
      cpu->pc += (int8_t)inst.b.offset * 2;
    }
    break;
  case OP_JMP:
    cpu->pc += (int8_t)inst.b.offset * 2;

    break;
  case OP_JALR: {
    uint16_t target = cpu->reg[inst.i.rb] & 0xFFFE;
    cpu->reg[inst.rd] = cpu->pc;
    cpu->pc = target;
    break;
  }
  case OP_HALT:
    cpu->halted = 1;
    break;
  default:
    break;
  }

  if (cpu->pc == old_pc) {
    printf("CPU cleanly halted on self-loop at PC: 0x%04X\n", old_pc);
    cpu->halted = 1;
  }
}
