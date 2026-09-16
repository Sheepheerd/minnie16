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
  OP_JAL,
  OP_EXT,
} Opcode;

// OP_EXT function codes (low 4 bits)
typedef enum {
  FN_HALT,
  FN_XOR,
  FN_SRL,
  FN_SRA,
  FN_SLTS,
  FN_LB,
  FN_SB,
} Funct;

typedef enum { FMT_R, FMT_I, FMT_L, FMT_B, FMT_J, FMT_X } Format;

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
      int8_t imm; // imm4 (LW/SW: word offset, address = rb + imm4 * 2)
    } i;
    struct {
      int8_t imm; // imm8
    } l;
    struct {
      int8_t offset;
    } b;
    struct {
      int16_t offset; // offset12
    } j;
    struct {
      uint8_t rs;
      uint8_t funct;
    } x;
  };

} Instruction;

Instruction decode(uint16_t raw);

Format decode_format(uint16_t raw);

#endif

