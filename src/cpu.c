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
  } else if (inst.fmt == FMT_J) {
    printf("Decoded: op=%d, offset=%d\n", inst.op, inst.j.offset);
  } else if (inst.fmt == FMT_X) {
    printf("Decoded: op=%d, rd=%d, rs=%d, funct=%d\n", inst.op, inst.rd,
           inst.x.rs, inst.x.funct);
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
    // Offset counts words: 0 to 15 words is 0 to 30 bytes
    uint16_t addr = cpu->reg[inst.i.rb] + inst.i.imm * 2;
    cpu->reg[inst.rd] = bus_read(bus, addr);
    break;
  }
  case OP_SW: {
    uint16_t addr = cpu->reg[inst.i.rb] + inst.i.imm * 2;
    uint16_t data = cpu->reg[inst.rd]; // rd holds source data for SW
    bus_write(bus, addr, data);
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
  case OP_JALR: {
    uint16_t target = cpu->reg[inst.i.rb] & 0xFFFE;
    cpu->reg[inst.rd] = cpu->pc;
    cpu->pc = target;
    break;
  }
  case OP_JAL:
    cpu->reg[15] = cpu->pc; // r15 is the link register
    cpu->pc += inst.j.offset * 2;
    break;
  case OP_EXT: {
    uint16_t a = cpu->reg[inst.rd];
    uint16_t b = cpu->reg[inst.x.rs];

    switch (inst.x.funct) {
    case FN_HALT:
      cpu->halted = 1;
      break;
    case FN_XOR:
      cpu->reg[inst.rd] = a ^ b;
      break;
    case FN_SRL:
      cpu->reg[inst.rd] = b >= 16 ? 0 : a >> b;
      break;
    case FN_SRA: {
      // Shift the complement so the result stays portable for negative values
      uint16_t amt = b >= 16 ? 15 : b;
      if (a & 0x8000) {
        cpu->reg[inst.rd] = (uint16_t)~((uint16_t)~a >> amt);
      } else {
        cpu->reg[inst.rd] = a >> amt;
      }
      break;
    }
    case FN_SLTS:
      cpu->reg[inst.rd] = (a ^ 0x8000) < (b ^ 0x8000);
      break;
    case FN_LB:
      cpu->reg[inst.rd] = bus_read8(bus, b);
      break;
    case FN_SB:
      bus_write8(bus, b, a & 0xFF); // rd holds source data for SB
      break;
    default:
      fprintf(stderr, "illegal EXT funct: 0x%X at PC: 0x%04X\n", inst.x.funct,
              old_pc);
      cpu->halted = 1;
      break;
    }
    break;
  }
  default:
    break;
  }

  if (cpu->pc == old_pc) {
    printf("CPU cleanly halted on self-loop at PC: 0x%04X\n", old_pc);
    cpu->halted = 1;
  }
}
