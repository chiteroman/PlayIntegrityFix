#pragma once

#if 0
enum LoadRegLiteralOp {
  LoadRegLiteralFixed = 0x18000000,
  LoadRegLiteralFixedMask = 0x3B000000,
  LoadRegLiteralMask = 0xFF000000,
};

// PC relative addressing.
enum PCRelAddressingOp {
  PCRelAddressingFixed = 0x10000000,
  PCRelAddressingFixedMask = 0x1F000000,
  PCRelAddressingMask = 0x9F000000,
  ADR = PCRelAddressingFixed | 0x00000000,
  ADRP = PCRelAddressingFixed | 0x80000000
};

// Unconditional branch.
enum UnconditionalBranchOp {
  UnconditionalBranchFixed = 0x14000000,
  UnconditionalBranchFixedMask = 0x7C000000,
  UnconditionalBranchMask = 0xFC000000,

  B = UnconditionalBranchFixed | 0x00000000,
  BL = UnconditionalBranchFixed | 0x80000000
};
#endif

// Compare and branch.
enum CompareBranchOp {
  CompareBranchFixed = 0x34000000,
  CompareBranchFixedMask = 0x7E000000,
  CompareBranchMask = 0xFF000000,
};

// Conditional branch.
enum ConditionalBranchOp {
  ConditionalBranchFixed = 0x54000000,
  ConditionalBranchFixedMask = 0xFE000000,
  ConditionalBranchMask = 0xFF000010,
};

// Test and branch.
enum TestBranchOp {
  TestBranchFixed = 0x36000000,
  TestBranchFixedMask = 0x7E000000,
  TestBranchMask = 0x7F000000,
};