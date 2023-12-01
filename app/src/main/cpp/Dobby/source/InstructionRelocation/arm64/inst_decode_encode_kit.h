#pragma once

#include "dobby/common.h"

static inline int64_t SignExtend(unsigned long x, int M, int N) {
#if 1
  char sign_bit = bit(x, M - 1);
  unsigned long sign_mask = 0 - sign_bit;
  x |= ((sign_mask >> M) << M);
#else
  x = (long)((long)x << (N - M)) >> (N - M);
#endif
  return (int64_t)x;
}

static inline int64_t decode_imm14_offset(uint32_t instr) {
  int64_t offset;
  {
    int64_t imm14 = bits(instr, 5, 18);
    offset = (imm14 << 2);
  }
  offset = SignExtend(offset, 2 + 14, 64);
  return offset;
}
static inline uint32_t encode_imm14_offset(uint32_t instr, int64_t offset) {
  uint32_t imm14 = bits((offset >> 2), 0, 13);
  set_bits(instr, 5, 18, imm14);
  return instr;
}

static inline int64_t decode_imm19_offset(uint32_t instr) {
  int64_t offset;
  {
    int64_t imm19 = bits(instr, 5, 23);
    offset = (imm19 << 2);
  }
  offset = SignExtend(offset, 2 + 19, 64);
  return offset;
}

static inline uint32_t encode_imm19_offset(uint32_t instr, int64_t offset) {
  uint32_t imm19 = bits((offset >> 2), 0, 18);
  set_bits(instr, 5, 23, imm19);
  return instr;
}

static inline int64_t decode_imm26_offset(uint32_t instr) {
  int64_t offset;
  {
    int64_t imm26 = bits(instr, 0, 25);
    offset = (imm26 << 2);
  }
  offset = SignExtend(offset, 2 + 26, 64);
  return offset;
}
static inline uint32_t encode_imm26_offset(uint32_t instr, int64_t offset) {
  uint32_t imm26 = bits((offset >> 2), 0, 25);
  set_bits(instr, 0, 25, imm26);
  return instr;
}

static inline int64_t decode_immhi_immlo_offset(uint32_t instr) {
  typedef uint32_t instr_t;
  struct {
    instr_t Rd : 5;      // Destination register
    instr_t immhi : 19;  // 19-bit upper immediate
    instr_t dummy_0 : 5; // Must be 10000 == 0x10
    instr_t immlo : 2;   // 2-bit lower immediate
    instr_t op : 1;      // 0 = ADR, 1 = ADRP
  } instr_decode;

  *(instr_t *)&instr_decode = instr;

  int64_t imm = instr_decode.immlo + (instr_decode.immhi << 2);
  imm = SignExtend(imm, 2 + 19, 64);
  return imm;
}
static inline uint32_t encode_immhi_immlo_offset(uint32_t instr, int64_t offset) {
  struct {
    uint32_t Rd : 5;      // Destination register
    uint32_t immhi : 19;  // 19-bit upper immediate
    uint32_t dummy_0 : 5; // Must be 10000 == 0x10
    uint32_t immlo : 2;   // 2-bit lower immediate
    uint32_t op : 1;      // 0 = ADR, 1 = ADRP
  } instr_decode;

  *(uint32_t *)&instr_decode = instr;
  instr_decode.immlo = bits(offset, 0, 2);
  instr_decode.immhi = bits(offset, 2, 2 + 19);

  return *(uint32_t *)&instr_decode;
}

static inline int64_t decode_immhi_immlo_zero12_offset(uint32_t instr) {
  int64_t imm = decode_immhi_immlo_offset(instr);
  imm = imm << 12;
  return imm;
}
static inline uint32_t encode_immhi_immlo_zero12_offset(uint32_t instr, int64_t offset) {
  offset = (offset >> 12);
  return encode_immhi_immlo_offset(instr, offset);
}

static inline int decode_rt(uint32_t instr) {
  return bits(instr, 0, 4);
}

static inline int decode_rd(uint32_t instr) {
  return bits(instr, 0, 4);
}