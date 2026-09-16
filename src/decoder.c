#include "decoder.h"
#include <stdio.h>
#include <stdlib.h>

Format decode_format(uint16_t opcode) {
  switch (opcode) {
  case OP_ADD:
  case OP_SUB:
  case OP_AND:
  case OP_OR:
  case OP_SLL:
  case OP_SLT:
  case OP_HALT:
    return FMT_R;

  case OP_ADDI:
  case OP_LW:
  case OP_SW:
  case OP_JALR:
    return FMT_I;

  case OP_LIL:
  case OP_LIH:
    return FMT_L;

  case OP_BEQZ:
  case OP_BNEZ:
  case OP_JMP:
    return FMT_B;

  default:
    fprintf(stderr, "illegal opcode: 0x%X\n", opcode);
    exit(1);
  }
}

Instruction decode(uint16_t raw) {
  Instruction inst = {0};

  inst.op = (raw >> 12) & 0xF;
  inst.fmt = decode_format(inst.op);
  inst.rd = (raw >> 8) & 0xF;

  switch (inst.fmt) {
  case FMT_R:
    inst.r.rs1 = (raw >> 4) & 0xF;
    inst.r.rs2 = raw & 0xF;
    break;

  case FMT_I:
    inst.i.rb = (raw >> 4) & 0xF;
    inst.i.imm = raw & 0xF;
    break;

  case FMT_L:
    inst.l.imm = raw & 0xFF;
    break;

  case FMT_B:
    inst.b.offset = (int8_t)(raw & 0xFF);
    break;
  }

  return inst;
}
