#ifndef DECODER_H
#define DECODER_H

#include <stdint.h>

typedef enum {
  // OPCODES
  OP_ADD,
  OP_SUB,
  OP_ADDI,
  OP_AND,
  OP_OR,
  OP_SLL,
  OP_SLT,
  OP_LW,
  OP_SW,
  OP_LIL,
  OP_LIH,
  OP_BEQZ,
  OP_BNEZ,
  OP_JALR,
  OP_JMP,
  OP_HALT,
} Opcode;

typedef enum { FMT_R, FMT_I, FMT_L, FMT_B } Format;

typedef struct {

  Opcode op;
  Format fmt;
  uint8_t rd;
  union {
    struct {
      uint8_t rs1;
      uint8_t rs2;
    } r;
    struct {
      uint8_t rb;
      int8_t imm; // imm4
    } i;
    struct {
      int8_t imm; // imm8
    } l;
    struct {
      int8_t offset;
    } b;
  };

} Instruction;

Instruction decode(uint16_t raw);

Format decode_format(uint16_t raw);

#endif

