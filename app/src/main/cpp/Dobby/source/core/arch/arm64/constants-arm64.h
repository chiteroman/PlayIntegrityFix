#ifndef CORE_ARCH_CONSTANTS_ARM64_H
#define CORE_ARCH_CONSTANTS_ARM64_H

#include "common_header.h"

enum Shift { NO_SHIFT = -1, LSL = 0x0, LSR = 0x1, ASR = 0x2, ROR = 0x3, MSL = 0x4 };

enum Extend { NO_EXTEND = -1, UXTB = 0, UXTH = 1, UXTW = 2, UXTX = 3, SXTB = 4, SXTH = 5, SXTW = 6, SXTX = 7 };

enum AddrMode { Offset, PreIndex, PostIndex };

enum FlagsUpdate { SetFlags = 1, LeaveFlags = 0 };

enum InstructionFields {

  // Registers.
  kRdShift = 0,
  kRdBits = 5,
  kRnShift = 5,
  kRnBits = 5,
  kRaShift = 10,
  kRaBits = 5,
  kRmShift = 16,
  kRmBits = 5,
  kRtShift = 0,
  kRtBits = 5,
  kRt2Shift = 10,
  kRt2Bits = 5,
  kRsShift = 16,
  kRsBits = 5,

};

#define OP(op) op
#define OP_W(op) op##_w
#define OP_X(op) op##_x
#define OP_B(op) op##_b
#define OP_H(op) op##_h
#define OP_S(op) op##_s
#define OP_D(op) op##_d
#define OP_Q(op) op##_q

#define OPT(op, attribute) op##_##attribute
#define OPT_W(op, attribute) op##_w_##attribute
#define OPT_X(op, attribute) op##_x_##attribute
#define OPT_B(op, attribute) op##_b_##attribute
#define OPT_H(op, attribute) op##_h_##attribute
#define OPT_S(op, attribute) op##_s_##attribute
#define OPT_D(op, attribute) op##_d_##attribute
#define OPT_Q(op, attribute) op##_q_##attribute

// =====

// Exception.
enum ExceptionOp {
  ExceptionFixed = 0xD4000000,
  ExceptionFMask = 0xFF000000,
  ExceptionMask = 0xFFE0001F,

  HLT = ExceptionFixed | 0x00400000,
  BRK = ExceptionFixed | 0x00200000,
  SVC = ExceptionFixed | 0x00000001,
  HVC = ExceptionFixed | 0x00000002,
  SMC = ExceptionFixed | 0x00000003,
  DCPS1 = ExceptionFixed | 0x00A00001,
  DCPS2 = ExceptionFixed | 0x00A00002,
  DCPS3 = ExceptionFixed | 0x00A00003
};

// =====

// Unconditional branch.
enum UnconditionalBranchOp {
  UnconditionalBranchFixed = 0x14000000,
  UnconditionalBranchFixedMask = 0x7C000000,
  UnconditionalBranchMask = 0xFC000000,

  B = UnconditionalBranchFixed | 0x00000000,
  BL = UnconditionalBranchFixed | 0x80000000
};

// =====

// Unconditional branch to register.
enum UnconditionalBranchToRegisterOp {
  UnconditionalBranchToRegisterFixed = 0xD6000000,
  UnconditionalBranchToRegisterFixedMask = 0xFE000000,
  UnconditionalBranchToRegisterMask = 0xFFFFFC1F,

  BR = UnconditionalBranchToRegisterFixed | 0x001F0000,
  BLR = UnconditionalBranchToRegisterFixed | 0x003F0000,
  RET = UnconditionalBranchToRegisterFixed | 0x005F0000
};

// =====

enum LoadRegLiteralOp {
  LoadRegLiteralFixed = 0x18000000,
  LoadRegLiteralFixedMask = 0x3B000000,
  LoadRegLiteralMask = 0xFF000000,

#define LoadRegLiteralSub(opc, V) LoadRegLiteralFixed | LeftShift(opc, 2, 30) | LeftShift(V, 1, 26)
  OPT_W(LDR, literal) = LoadRegLiteralSub(0b00, 0),
  OPT_X(LDR, literal) = LoadRegLiteralSub(0b01, 0),
  OPT(LDRSW, literal) = LoadRegLiteralSub(0b10, 0),
  OPT(PRFM, literal) = LoadRegLiteralSub(0b11, 0),
  OPT_S(LDR, literal) = LoadRegLiteralSub(0b00, 1),
  OPT_D(LDR, literal) = LoadRegLiteralSub(0b01, 1),
  OPT_Q(LDR, literal) = LoadRegLiteralSub(0b10, 1),
};

// =====

// clang-format off
#define LOAD_STORE_OP_LIST(V)   \
  V(OP_W(STRB),   0b00, 0, 0b00),   \
  V(OP_W(LDRB),   0b00, 0, 0b01),   \
  V(OP_X(LDRSB),  0b00, 0, 0b10),   \
  V(OP_W(LDRSB),  0b00, 0, 0b11),   \
  V(OP_B(STR),    0b00, 1, 0b00),   \
  V(OP_B(LDR),    0b00, 1, 0b01),   \
  V(OP_Q(STR),    0b00, 1, 0b10),   \
  V(OP_Q(LDR),    0b00, 1, 0b11),   \
  V(OP_W(STRH),   0b01, 0, 0b00),   \
  V(OP_W(LDRH),   0b01, 0, 0b01),   \
  V(OP_X(LDRSH),  0b01, 0, 0b10),   \
  V(OP_W(LDRSH),  0b01, 0, 0b11),   \
  V(OP_H(STR),    0b01, 1, 0b00),   \
  V(OP_H(LDR),    0b01, 1, 0b01),   \
  V(OP_W(STR),    0b10, 0, 0b00),   \
  V(OP_W(LDR),    0b10, 0, 0b01),   \
  V(OP(LDRSW),    0b10, 0, 0b10),   \
  V(OP_S(STR),    0b10, 1, 0b00),   \
  V(OP_S(LDR),    0b10, 1, 0b01),   \
  V(OP_X(STR),    0b11, 0, 0b00),   \
  V(OP_X(LDR),    0b11, 0, 0b01),   \
  V(OP(PRFM),     0b11, 0, 0b10),   \
  V(OP_D(STR),    0b11, 1, 0b00),   \
  V(OP_D(LDR),    0b11, 1, 0b01),
// clang-format on

// Load/store
enum LoadStoreOp {
#define LoadStoreOpSub(size, V, opc) LeftShift(size, 2, 30) | LeftShift(V, 1, 26) | LeftShift(opc, 2, 22)
#define LOAD_STORE(opname, size, V, opc) OP(opname) = LoadStoreOpSub(size, V, opc)
  LOAD_STORE_OP_LIST(LOAD_STORE)
#undef LOAD_STORE
};

// Load/store register offset.
enum LoadStoreRegisterOffsetOp {
  LoadStoreRegisterOffsetFixed = 0x38200800,
  LoadStoreRegisterOffsetFixedMask = 0x3B200C00,
  LoadStoreRegisterOffsetMask = 0xFFE00C00,

#define LoadStoreRegisterOffsetOpSub(size, V, opc)                                                                     \
  LoadStoreRegisterOffsetFixed | LeftShift(size, 2, 30) | LeftShift(V, 1, 26) | LeftShift(opc, 2, 22)
#define LOAD_STORE_REGISTER_OFFSET(opname, size, V, opc)                                                               \
  OPT(opname, register) = LoadStoreRegisterOffsetOpSub(size, V, opc)
  LOAD_STORE_OP_LIST(LOAD_STORE_REGISTER_OFFSET)
#undef LOAD_STORE_REGISTER_OFFSET
};

// Load/store register (unscaled immediate)
enum LoadStoreUnscaledOffsetOp {
  LoadStoreUnscaledOffsetFixed = 0x38000000,
  LoadStoreUnscaledOffsetFixedMask = 0x3B200C00,
  LoadStoreUnscaledOffsetMask = 0xFFE00C00,

#define LoadStoreUnscaledOffsetOpSub(size, V, opc)                                                                     \
  LoadStoreUnscaledOffsetFixed | LeftShift(size, 2, 30) | LeftShift(V, 1, 26) | LeftShift(opc, 2, 22)
#define LOAD_STORE_UNSCALED(opname, size, V, opc) OPT(opname, unscaled) = LoadStoreUnscaledOffsetOpSub(size, V, opc)
  LOAD_STORE_OP_LIST(LOAD_STORE_UNSCALED)
#undef LOAD_STORE_UNSCALED
};

// Load/store unsigned offset.
enum LoadStoreUnsignedOffset {
  LoadStoreUnsignedOffsetFixed = 0x39000000,
  LoadStoreUnsignedOffsetFixedMask = 0x3B000000,
  LoadStoreUnsignedOffsetMask = 0xFFC00000,

#define LoadStoreUnsignedOffsetSub(size, V, opc)                                                                       \
  LoadStoreUnsignedOffsetFixed | LeftShift(size, 2, 30) | LeftShift(V, 1, 26) | LeftShift(opc, 2, 22)
#define LOAD_STORE_UNSIGNED_OFFSET(opname, size, V, opc)                                                               \
  OPT(opname, unsigned) = LoadStoreUnsignedOffsetSub(size, V, opc)
  LOAD_STORE_OP_LIST(LOAD_STORE_UNSIGNED_OFFSET)
#undef LOAD_STORE_UNSIGNED_OFFSET
};

// =====

// clang-format off
#define LOAD_STORE_PAIR_OP_LIST(V) \
  V(OP_W(STP),    0b00, 0, 0),   \
  V(OP_W(LDP),    0b00, 0, 1),   \
  V(OP_S(STP),    0b00, 1, 0),   \
  V(OP_S(LDP),    0b00, 1, 1),   \
  V(OP(LDPSW),    0b01, 0, 1),   \
  V(OP_D(STP),    0b01, 1, 0),   \
  V(OP_D(LDP),    0b01, 1, 1),   \
  V(OP_X(STP),    0b10, 0, 0),   \
  V(OP_X(LDP),    0b10, 0, 1),   \
  V(OP_Q(STP),    0b10, 1, 0),   \
  V(OP_Q(LDP),    0b10, 1, 1)
// clang-format on

enum LoadStorePairOp {
#define LoadStorePairOpSub(opc, V, L) LeftShift(opc, 2, 30) | LeftShift(V, 1, 26) | LeftShift(L, 1, 22)
#define LOAD_STORE_PAIR(opname, opc, V, L) OP(opname) = LoadStorePairOpSub(opc, V, L)
  LOAD_STORE_PAIR_OP_LIST(LOAD_STORE_PAIR)
#undef LOAD_STORE_PAIR
};

enum LoadStorePairOffsetOp {
  LoadStorePairOffsetFixed = 0x29000000,
  LoadStorePairOffsetFixedMask = 0x3B800000,
  LoadStorePairOffsetMask = 0xFFC00000,

#define LoadStorePairOffsetOpSub(opc, V, L)                                                                            \
  LoadStorePairOffsetFixed | LeftShift(opc, 2, 30) | LeftShift(V, 1, 26) | LeftShift(L, 1, 22)
#define LOAD_STORE_PAIR_OFFSET(opname, opc, V, L) OPT(opname, offset) = LoadStorePairOffsetOpSub(opc, V, L)
  LOAD_STORE_PAIR_OP_LIST(LOAD_STORE_PAIR_OFFSET)
#undef LOAD_STORE_PAIR_OFFSET
};

enum LoadStorePairPostIndexOp {
  LoadStorePairPostIndexFixed = 0x28800000,
  LoadStorePairPostIndexFixedMask = 0x3B800000,
  LoadStorePairPostIndexMask = 0xFFC00000,

#define LoadStorePairPostOpSub(opc, V, L)                                                                              \
  LoadStorePairPostIndexFixed | LeftShift(opc, 2, 30) | LeftShift(V, 1, 26) | LeftShift(L, 1, 22)
#define LOAD_STORE_PAIR_POST_INDEX(opname, opc, V, L) OPT(opname, post) = LoadStorePairPostOpSub(opc, V, L)
  LOAD_STORE_PAIR_OP_LIST(LOAD_STORE_PAIR_POST_INDEX)
#undef LOAD_STORE_PAIR_POST_INDEX
};

enum LoadStorePairPreIndexOp {
  LoadStorePairPreIndexFixed = 0x29800000,
  LoadStorePairPreIndexFixedMask = 0x3B800000,
  LoadStorePairPreIndexMask = 0xFFC00000,

#define LoadStorePairPreOpSub(opc, V, L)                                                                               \
  LoadStorePairPreIndexFixed | LeftShift(opc, 2, 30) | LeftShift(V, 1, 26) | LeftShift(L, 1, 22)
#define LOAD_STORE_PAIR_PRE_INDEX(opname, opc, V, L) OPT(opname, pre) = LoadStorePairPreOpSub(opc, V, L)
  LOAD_STORE_PAIR_OP_LIST(LOAD_STORE_PAIR_PRE_INDEX)
#undef LOAD_STORE_PAIR_PRE_INDEX
};

// =====

// Generic fields.
enum GenericInstrField { SixtyFourBits = 0x80000000, ThirtyTwoBits = 0x00000000, FP32 = 0x00000000, FP64 = 0x00400000 };

// Generic utils
// #define sf(rd) (rd.Is64Bits() ? SixtyFourBits : ThirtyTwoBits)

// =====

// Move wide immediate.
enum MoveWideImmediateOp {
  MoveWideImmediateFixed = 0x12800000,
  MoveWideImmediateFixedMask = 0x1F800000,
  MoveWideImmediateMask = 0xFF800000,

  OP(MOVN) = 0x00000000,
  OP(MOVZ) = 0x40000000,
  OP(MOVK) = 0x60000000,

#define MoveWideImmediateOpSub(sf, opc) MoveWideImmediateFixed | LeftShift(sf, 1, 31) | LeftShift(opc, 2, 29)
  OP_W(MOVN) = MoveWideImmediateFixed | MOVN,
  OP_X(MOVN) = MoveWideImmediateFixed | MOVN | SixtyFourBits,
  OP_W(MOVZ) = MoveWideImmediateFixed | MOVZ,
  OP_X(MOVZ) = MoveWideImmediateFixed | MOVZ | SixtyFourBits,
  OP_W(MOVK) = MoveWideImmediateFixed | MOVK,
  OP_X(MOVK) = MoveWideImmediateFixed | MOVK | SixtyFourBits
};

// =====

enum AddSubImmediateOp {
  AddSubImmediateFixed = 0x11000000,
  AddSubImmediateFixedMask = 0x1F000000,
  AddSubImmediateMask = 0xFF000000,

#define AddSubImmediateOpSub(sf, op, S)                                                                                \
  AddSubImmediateFixed | LeftShift(sf, 1, 31) | LeftShift(op, 1, 30) | LeftShift(S, 1, 29)
  OPT_W(ADD, imm) = AddSubImmediateOpSub(0, 0, 0),
  OPT_W(ADDS, imm) = AddSubImmediateOpSub(0, 0, 1),
  OPT_W(SUB, imm) = AddSubImmediateOpSub(0, 1, 0),
  OPT_W(SUBS, imm) = AddSubImmediateOpSub(0, 1, 1),
  OPT_X(ADD, imm) = AddSubImmediateOpSub(1, 0, 0),
  OPT_X(ADDS, imm) = AddSubImmediateOpSub(1, 0, 1),
  OPT_X(SUB, imm) = AddSubImmediateOpSub(1, 1, 0),
  OPT_X(SUBS, imm) = AddSubImmediateOpSub(1, 1, 1)
};

enum AddSubShiftedOp {
  AddSubShiftedFixed = 0x0B000000,
  AddSubShiftedFixedMask = 0x1F200000,
  AddSubShiftedMask = 0xFF200000,

#define AddSubShiftedOpSub(sf, op, S)                                                                                  \
  AddSubShiftedFixed | LeftShift(sf, 1, 31) | LeftShift(op, 1, 30) | LeftShift(S, 1, 29)
  OPT_W(ADD, shift) = AddSubShiftedOpSub(0, 0, 0),
  OPT_W(ADDS, shift) = AddSubShiftedOpSub(0, 0, 1),
  OPT_W(SUB, shift) = AddSubShiftedOpSub(0, 1, 0),
  OPT_W(SUBS, shift) = AddSubShiftedOpSub(0, 1, 1),
  OPT_X(ADD, shift) = AddSubShiftedOpSub(1, 0, 0),
  OPT_X(ADDS, shift) = AddSubShiftedOpSub(1, 0, 1),
  OPT_X(SUB, shift) = AddSubShiftedOpSub(1, 1, 0),
  OPT_X(SUBS, shift) = AddSubShiftedOpSub(1, 1, 1)
};

enum AddSubExtendedOp {
  AddSubExtendedFixed = 0x0B200000,
  AddSubExtendedFixedMask = 0x1F200000,
  AddSubExtendedMask = 0xFFE00000,

#define AddSubExtendedOpSub(sf, op, S)                                                                                 \
  AddSubExtendedFixed | LeftShift(sf, 1, 31) | LeftShift(op, 1, 30) | LeftShift(S, 1, 29)
  OPT_W(ADD, extend) = AddSubExtendedOpSub(0, 0, 0),
  OPT_W(ADDS, extend) = AddSubExtendedOpSub(0, 0, 1),
  OPT_W(SUB, extend) = AddSubExtendedOpSub(0, 1, 0),
  OPT_W(SUBS, extend) = AddSubExtendedOpSub(0, 1, 1),
  OPT_X(ADD, extend) = AddSubExtendedOpSub(1, 0, 0),
  OPT_X(ADDS, extend) = AddSubExtendedOpSub(1, 0, 1),
  OPT_X(SUB, extend) = AddSubExtendedOpSub(1, 1, 0),
  OPT_X(SUBS, extend) = AddSubExtendedOpSub(1, 1, 1)
};

// =====

// Logical (immediate and shifted register).
enum LogicalOp {
  LogicalOpMask = 0x60200000,
  NOT = 0x00200000,
  AND = 0x00000000,
  BIC = AND | NOT,
  ORR = 0x20000000,
  ORN = ORR | NOT,
  EOR = 0x40000000,
  EON = EOR | NOT,
  ANDS = 0x60000000,
  BICS = ANDS | NOT
};

// Logical immediate.
enum LogicalImmediateOp {
  LogicalImmediateFixed = 0x12000000,
  LogicalImmediateFixedMask = 0x1F800000,
  LogicalImmediateMask = 0xFF800000,

#define W_X_OP(opname, combine_fields)                                                                                 \
  OPT_W(opname, imm) = LogicalImmediateFixed | combine_fields | ThirtyTwoBits,                                         \
                OPT_X(opname, imm) = LogicalImmediateFixed | combine_fields | SixtyFourBits
#define W_X_OP_LIST(V) V(AND, AND), V(ORR, ORR), V(EOR, EOR), V(ANDS, ANDS)
#undef W_X_OP
#undef W_X_OP_LIST
};

// Logical shifted register.
enum LogicalShiftedOp {
  LogicalShiftedFixed = 0x0A000000,
  LogicalShiftedFixedMask = 0x1F000000,
  LogicalShiftedMask = 0xFF200000,

#define W_X_OP(opname, combine_fields)                                                                                 \
  OPT_W(opname, shift) = LogicalShiftedFixed | combine_fields | ThirtyTwoBits,                                         \
                OPT_X(opname, shift) = LogicalShiftedFixed | combine_fields | SixtyFourBits
#define W_X_OP_LIST(V)                                                                                                 \
  V(AND, AND), V(BIC, BIC), V(ORR, ORR), V(ORN, ORN), V(EOR, EOR), V(EON, EON), V(ANDS, ANDS), V(BICS, BICS)
#undef W_X_OP
#undef W_X_OP_LIST
};

// PC relative addressing.
enum PCRelAddressingOp {
  PCRelAddressingFixed = 0x10000000,
  PCRelAddressingFixedMask = 0x1F000000,
  PCRelAddressingMask = 0x9F000000,
  ADR = PCRelAddressingFixed | 0x00000000,
  ADRP = PCRelAddressingFixed | 0x80000000
};

#endif
