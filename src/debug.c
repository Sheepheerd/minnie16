#include "debug.h"
#include "decoder.h"
#include <stdio.h>

static const char *R_NAMES[] = {"ADD", "SUB", "ADDI", "AND", "OR", "SLL", "SLT"};
static const char *X_NAMES[] = {"HALT", "XOR", "SRL", "SRA", "SLTS", "LB", "SB"};

void disassemble(uint16_t raw, uint16_t pc, char *buf, size_t n) {
  Instruction inst = decode(raw);
  uint16_t next = pc + 2;

  switch (inst.op) {
  case OP_ADD:
    // Show the pseudo-instructions the assembler expands into ADD
    if (inst.rd == 0 && inst.r.rs1 == 0 && inst.r.rs2 == 0) {
      snprintf(buf, n, "NOP");
    } else if (inst.r.rs2 == 0) {
      snprintf(buf, n, "MOV  r%d, r%d", inst.rd, inst.r.rs1);
    } else {
      snprintf(buf, n, "ADD  r%d, r%d, r%d", inst.rd, inst.r.rs1, inst.r.rs2);
    }
    break;
  case OP_SUB:
  case OP_AND:
  case OP_OR:
  case OP_SLL:
  case OP_SLT:
    snprintf(buf, n, "%-4s r%d, r%d, r%d", R_NAMES[inst.op], inst.rd,
             inst.r.rs1, inst.r.rs2);
    break;
  case OP_ADDI: {
    int imm = (inst.i.imm ^ 0x8) - 0x8; // sign extend imm4
    snprintf(buf, n, "ADDI r%d, r%d, %d", inst.rd, inst.i.rb, imm);
    break;
  }
  case OP_LW:
  case OP_SW:
    snprintf(buf, n, "%s   r%d, %d(r%d)", inst.op == OP_LW ? "LW" : "SW",
             inst.rd, inst.i.imm * 2, inst.i.rb);
    break;
  case OP_LIL:
  case OP_LIH:
    snprintf(buf, n, "%s  r%d, 0x%02X", inst.op == OP_LIL ? "LIL" : "LIH",
             inst.rd, (uint8_t)inst.l.imm);
    break;
  case OP_BEQZ:
  case OP_BNEZ: {
    char ops[24];
    if (inst.op == OP_BEQZ && inst.rd == 0) {
      snprintf(ops, sizeof(ops), "JMP  %d", inst.b.offset);
    } else {
      snprintf(ops, sizeof(ops), "%s r%d, %d",
               inst.op == OP_BEQZ ? "BEQZ" : "BNEZ", inst.rd, inst.b.offset);
    }
    snprintf(buf, n, "%-17s ; -> 0x%04X", ops,
             (uint16_t)(next + inst.b.offset * 2));
    break;
  }
  case OP_JALR:
    if (inst.rd == 0 && inst.i.rb == 15) {
      snprintf(buf, n, "RET");
    } else {
      snprintf(buf, n, "JALR r%d, r%d", inst.rd, inst.i.rb);
    }
    break;
  case OP_JAL: {
    char ops[24];
    snprintf(ops, sizeof(ops), "JAL  %d", inst.j.offset);
    snprintf(buf, n, "%-17s ; -> 0x%04X", ops,
             (uint16_t)(next + inst.j.offset * 2));
    break;
  }
  case OP_EXT:
    if (inst.x.funct == FN_HALT) {
      snprintf(buf, n, "HALT");
    } else if (inst.x.funct == FN_LB || inst.x.funct == FN_SB) {
      snprintf(buf, n, "%s   r%d, (r%d)", X_NAMES[inst.x.funct], inst.rd,
               inst.x.rs);
    } else if (inst.x.funct <= FN_SB) {
      snprintf(buf, n, "%-4s r%d, r%d", X_NAMES[inst.x.funct], inst.rd,
               inst.x.rs);
    } else {
      snprintf(buf, n, "???  (reserved EXT funct 0x%X)", inst.x.funct);
    }
    break;
  }
}
