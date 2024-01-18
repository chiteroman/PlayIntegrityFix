#include "platform_macro.h"
#if defined(TARGET_ARCH_IA32) || defined(TARGET_ARCH_X64)

#include "./X86OpcodoDecodeTable.h"

// clang-format on

#define _xUnknownOpHanlder -1, -1, OpSz_0, ImmSz_0, _UnknownOpHanlder
void _UnknownOpHanlder(InstrMnemonic *instr, addr_t p) {
  // printf("Unknown Operand\n");
  return;
}

#define _xInvalidOpHanlder -1, -1, OpSz_0, ImmSz_0, _InValidOpHanlder
void _InValidOpHanlder(InstrMnemonic *instr, addr_t p) {
  // printf("Invalid Operand\n");
  return;
}

inline void _ContinueDispatch(InstrMnemonic *instr, addr_t p) {
  OpcodeDecodeItem *item = &OpcodeDecodeTable[*(unsigned char *)p];
  item->DecodeHandler(instr, p);
}

// ===== Decode LegacyPrefix and REXPrefix =====

// clang-format off
#define _xDecodePrefix_0F 1, OpEn_NONE, OpSz_0, ImmSz_0, _DecodeLegacyPrefix
#define _xDecodePrefix_66 1, OpEn_NONE, OpSz_0, ImmSz_0, _DecodePrefix_66
#define _xDecodePrefix_67 1, OpEn_NONE, OpSz_0, ImmSz_0, _DecodeLegacyPrefix
#define _xDecodeREXPrefix 1, OpEn_NONE, OpSz_0, ImmSz_0, _DecodeREXPrefix
#define _xDecodePrefix    1, OpEn_NONE, OpSz_0, ImmSz_0, _DecodeLegacyPrefix
#define _xDecodeSegPrefix 1, OpEn_NONE, OpSz_0, ImmSz_0, _DecodeLegacyPrefix
// clang-format on

void _DecodeREXPrefix(InstrMnemonic *instr, addr_t p) {
  instr->instr.REX = *(byte_t *)p;
  instr->len++;
  instr->OperandSz = OpSz_64;

  _ContinueDispatch(instr, p + 1); // continue decode
}

void _DecodeLegacyPrefix(InstrMnemonic *instr, addr_t p) {
  instr->instr.prefix = *(byte_t *)p;
  instr->len++;

  _ContinueDispatch(instr, p + 1); // continue decode
}

void _DecodePrefix_66(InstrMnemonic *instr, addr_t p) {
  instr->OperandSz = OpSz_16;
  _DecodeLegacyPrefix(instr, p);
}

// ===== Decode Opcode =====

static void _DecodeOp(InstrMnemonic *instr, addr_t p) {
  instr->instr.opcode1 = *(byte_t *)p;
  instr->len++;
}

static void _DecodeOpExtraOp(InstrMnemonic *instr, addr_t p) {
  _DecodeOp(instr, p);
}

static void _DecodeOpWithReg(InstrMnemonic *instr, addr_t p) {
  _DecodeOp(instr, p);
}

#define _xDecodeOpEn_ZO 1, OpEn_ZO, OpSz_0, ImmSz_0, _DecodeOpEn_ZO
void _DecodeOpEn_ZO(InstrMnemonic *instr, addr_t p) {
  _DecodeOp(instr, p);
}

#define _xDecodeOpEn_O 1, OpEn_O, OpSz_0, ImmSz_0, _DecodeOpEn_O
void _DecodeOpEn_O(InstrMnemonic *instr, addr_t p) {
  _DecodeOpWithReg(instr, p);
}

// ===== Decode Operand =====

// ===== Decode ModRM Operand =====

#define REX_W(byte) ((byte & 0b00001000) >> 3)
#define REX_R(byte) ((byte & 0b00000100) >> 2)
#define REX_X(byte) ((byte & 0b00000010) >> 1)
#define REX_B(byte) ((byte & 0b00000001) >> 0)

#define ModRM_Mod(byte) ((byte & 0b11000000) >> 6)
#define ModRM_RegOpcode(byte) ((byte & 0b00111000) >> 3)
#define ModRM_RM(byte) (byte & 0b00000111)

#define SIB_Scale(sib) ((sib & 0b11000000) >> 6)
#define SIB_Index(sib) ((sib & 0b00111000) >> 3)
#define SIB_Base(sib) ((sib & 0b00000111) >> 0)

#define REX_SIB_Base(rex, sib) ((REX_B(rex) << 3) | SIB_Base(sib))

void _DecodeDisplacement8(InstrMnemonic *instr, addr_t p) {
  *(byte_t *)&instr->instr.Displacement = *(byte_t *)p;
  instr->len += 1;
}

void _DecodeDisplacement32(InstrMnemonic *instr, addr_t p) {
  instr->instr.DisplacementOffset = instr->len;
  *(dword *)&instr->instr.Displacement = *(byte_t *)p;
  instr->len += 4;
}

void _DecodeSIB(InstrMnemonic *instr, addr_t p) {
  instr->instr.SIB = *(byte_t *)p;
  instr->len++;
}

void _DecodeModRM(InstrMnemonic *instr, addr_t p) {
  int init_len = instr->len;

  instr->instr.ModRM = *(byte_t *)p;
  instr->len++;

#if defined(_M_X64) || defined(__x86_64__)
  if (ModRM_Mod(instr->instr.ModRM) == 0b00 && ModRM_RM(instr->instr.ModRM) == 0b101) {
    // RIP-Relative Addressing
    instr->flag = instr->flag | kIPRelativeAddress;
    _DecodeDisplacement32(instr, p + (instr->len - init_len));
    return;
  }
#endif

  // Addressing Forms with the SIB Byte
  if (ModRM_Mod(instr->instr.ModRM) != 0b11 && ModRM_RM(instr->instr.ModRM) == 0b100) {
    _DecodeSIB(instr, p + (instr->len - init_len));
  }

  // [REG]
  if (ModRM_Mod(instr->instr.ModRM) == 0b00) {
    if (ModRM_RM(instr->instr.ModRM) == 0b101) {
      _DecodeDisplacement32(instr, p + (instr->len - init_len));
      return;
    }
  }

  // [REG+disp8}
  if (ModRM_Mod(instr->instr.ModRM) == 0b01) {
    _DecodeDisplacement8(instr, p + (instr->len - init_len));
    return;
  }

  // [REG+disp32}
  if (ModRM_Mod(instr->instr.ModRM) == 0b10) {
    _DecodeDisplacement32(instr, p + (instr->len - init_len));
    return;
  }

  // REG
  if (ModRM_Mod(instr->instr.ModRM) == 0b11) {
  }
}

void _DecodeOpEn_M(InstrMnemonic *instr, addr_t p) {
  _DecodeOp(instr, p);
  _DecodeModRM(instr, p + 1);
}

void _DecodeOpEn_RM(InstrMnemonic *instr, addr_t p) {
  _DecodeOp(instr, p);
  _DecodeModRM(instr, p + 1);
}

void _DecodeOpEn_MR(InstrMnemonic *instr, addr_t p) {
  _DecodeOp(instr, p);
  _DecodeModRM(instr, p + 1);
}

void _DecodeOpEn_M1(InstrMnemonic *instr, addr_t p) {
  _DecodeOp(instr, p);
  _DecodeModRM(instr, p + 1);
}

void _DecodeOpEn_MC(InstrMnemonic *instr, addr_t p) {
  _DecodeOp(instr, p);
  _DecodeModRM(instr, p + 1);
}

// ===== Decode Immediate Operand =====

void _DecodeImmedite(InstrMnemonic *instr, addr_t p, int sz) {

  instr->instr.ImmediateOffset = instr->len;

  OpcodeDecodeItem *item = &OpcodeDecodeTable[instr->instr.opcode1];
  if (sz == ImmSz_0) {
    sz = item->ImmediteSz;
    if (sz == (ImmSz_16 | ImmSz_32)) {
      if (instr->instr.prefix == 0x66) {
        sz = ImmSz_16;
      } else {
        sz = ImmSz_32; // Default Immedite Size
      }
    }
  }

  if (sz == ImmSz_8) {
    *(byte_t *)&instr->instr.Immediate = *(byte_t *)p;
    instr->len += 1;
  } else if (sz == ImmSz_16) {
    *(word *)&instr->instr.Immediate = *(dword *)p;
    instr->len += 2;
  } else if (sz == ImmSz_32) {
    *(dword *)&instr->instr.Immediate = *(dword *)p;
    instr->len += 4;
  }
}

void _DecodeOpEn_I(InstrMnemonic *instr, addr_t p) {
  _DecodeOp(instr, p);
  _DecodeImmedite(instr, p + 1, instr->ImmediteSz);
}

void _DecodeOpEn_OI(InstrMnemonic *instr, addr_t p) {
  _DecodeOpWithReg(instr, p);
  _DecodeImmedite(instr, p + 1, instr->ImmediteSz);
}

void _DecodeOpEn_D(InstrMnemonic *instr, addr_t p) {
  _DecodeOp(instr, p);
  _DecodeImmedite(instr, p + 1, instr->ImmediteSz);
}

// ===== Decode ModRM Immediate Operand =====

void _DecodeOpEn_RMI(InstrMnemonic *instr, addr_t p) {
  _DecodeOp(instr, p);
  _DecodeModRM(instr, p + 1);
  _DecodeImmedite(instr, p + 2, instr->ImmediteSz);
}

void _DecodeOpEn_MI(InstrMnemonic *instr, addr_t p) {
  _DecodeOpExtraOp(instr, p);
  _DecodeModRM(instr, p + 1);
  _DecodeImmedite(instr, p + 2, instr->ImmediteSz);
}

// ===== Decode Specific Opcode =====

#define _xDecodeOpC8 1, 0, OpSz_0, ImmSz_0, _DecodeOpC8
void _DecodeOpC8(InstrMnemonic *instr, addr_t p) {
  _DecodeOp(instr, p);

  instr->len = instr->len + 2 + 1;
}

// http://ref.x86asm.net/coder.html#x04
OpcodeDecodeItem OpcodeDecodeTable[257] = {

    {0x00, 2, OpEn_MR, OpSz_8, ImmSz_0, _DecodeOpEn_MR},
    {0x01, 2, OpEn_MR, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_MR},
    {0x02, 2, OpEn_RM, OpSz_8, ImmSz_0, _DecodeOpEn_RM},
    {0x03, 2, OpEn_RM, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_RM},
    {0x04, 1, OpEn_I, OpSz_0, ImmSz_8, _DecodeOpEn_I},
    {0x05, 1, OpEn_I, OpSz_16 | OpSz_32, ImmSz_16 | ImmSz_32, _DecodeOpEn_I},
#if defined(_M_X64) || defined(__x86_64__)
    {0x06, _xInvalidOpHanlder},
    {0x07, _xInvalidOpHanlder},
#else
    {0x06, _xDecodeOpEn_ZO},
    {0x07, _xDecodeOpEn_ZO},
#endif
    {0x08, 2, OpEn_MR, OpSz_8, ImmSz_0, _DecodeOpEn_MR},
    {0x09, 2, OpEn_MR, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_MR},
    {0x0A, 2, OpEn_RM, OpSz_8, ImmSz_0, _DecodeOpEn_RM},
    {0x0B, 2, OpEn_RM, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_RM},
    {0x0C, 1, OpEn_I, OpSz_0, ImmSz_8, _DecodeOpEn_I},
    {0x0D, 1, OpEn_I, OpSz_16 | OpSz_32, ImmSz_16 | ImmSz_32, _DecodeOpEn_I},
#if defined(_M_X64) || defined(__x86_64__)
    {0x0E, _xInvalidOpHanlder},
#else
    {0x0E, _xDecodeOpEn_ZO},
#endif
    {0x0F, _xDecodePrefix_0F},
    {0x10, 2, OpEn_MR, OpSz_8, ImmSz_0, _DecodeOpEn_MR},
    {0x11, 2, OpEn_MR, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_MR},
    {0x12, 2, OpEn_RM, OpSz_8, ImmSz_0, _DecodeOpEn_RM},
    {0x13, 2, OpEn_RM, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_RM},
    {0x14, 1, OpEn_I, OpSz_0, ImmSz_8, _DecodeOpEn_I},
    {0x15, 1, OpEn_I, OpSz_16 | OpSz_32, ImmSz_16 | ImmSz_32, _DecodeOpEn_I},
#if defined(_M_X64) || defined(__x86_64__)
    {0x16, _xInvalidOpHanlder},
    {0x17, _xInvalidOpHanlder},
#else
    {0x16, _xDecodeOpEn_ZO},
    {0x17, _xDecodeOpEn_ZO},
#endif
    {0x18, 2, OpEn_MR, OpSz_8, ImmSz_0, _DecodeOpEn_MR},
    {0x19, 2, OpEn_MR, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_MR},
    {0x1A, 2, OpEn_RM, OpSz_8, ImmSz_0, _DecodeOpEn_RM},
    {0x1B, 2, OpEn_RM, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_RM},
    {0x1C, 1, OpEn_I, OpSz_0, ImmSz_8, _DecodeOpEn_I},
    {0x1D, 1, OpEn_I, OpSz_16 | OpSz_32, ImmSz_16 | ImmSz_32, _DecodeOpEn_I},
#if defined(_M_X64) || defined(__x86_64__)
    {0x1E, _xInvalidOpHanlder},
    {0x1F, _xInvalidOpHanlder},
#else
    {0x1E, _xDecodeOpEn_ZO},
    {0x1F, _xDecodeOpEn_ZO},
#endif
    {0x20, 2, OpEn_MR, OpSz_8, ImmSz_0, _DecodeOpEn_MR},
    {0x21, 2, OpEn_MR, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_MR},
    {0x22, 2, OpEn_RM, OpSz_8, ImmSz_0, _DecodeOpEn_RM},
    {0x23, 2, OpEn_RM, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_RM},
    {0x24, 1, OpEn_I, OpSz_0, ImmSz_8, _DecodeOpEn_I},
    {0x25, 1, OpEn_I, OpSz_16 | OpSz_32, ImmSz_16 | ImmSz_32, _DecodeOpEn_I},
    {0x26, _xDecodeSegPrefix},
#if defined(_M_X64) || defined(__x86_64__)
    {0x27, _xInvalidOpHanlder},
#else
    {0x27, _xDecodeOpEn_ZO},
#endif
    {0x28, 2, OpEn_MR, OpSz_8, ImmSz_0, _DecodeOpEn_MR},
    {0x29, 2, OpEn_MR, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_MR},
    {0x2A, 2, OpEn_RM, OpSz_8, ImmSz_0, _DecodeOpEn_RM},
    {0x2B, 2, OpEn_RM, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_RM},
    {0x2C, 1, OpEn_I, OpSz_0, ImmSz_8, _DecodeOpEn_I},
    {0x2D, 1, OpEn_I, OpSz_16 | OpSz_32, ImmSz_16 | ImmSz_32, _DecodeOpEn_I},
    {0x2E, _xDecodeSegPrefix},
#if defined(_M_X64) || defined(__x86_64__)
    {0x2F, _xInvalidOpHanlder},
#else
    {0x2F, _xDecodeOpEn_ZO},
#endif
    {0x30, 2, OpEn_MR, OpSz_8, ImmSz_0, _DecodeOpEn_MR},
    {0x31, 2, OpEn_MR, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_MR},
    {0x32, 2, OpEn_RM, OpSz_8, ImmSz_0, _DecodeOpEn_RM},
    {0x33, 2, OpEn_RM, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_RM},
    {0x34, 1, OpEn_I, OpSz_0, ImmSz_8, _DecodeOpEn_I},
    {0x35, 1, OpEn_I, OpSz_16 | OpSz_32, ImmSz_16 | ImmSz_32, _DecodeOpEn_I},
    {0x36, _xDecodeSegPrefix},
#if defined(_M_X64) || defined(__x86_64__)
    {0x37, _xInvalidOpHanlder},
#else
    {0x37, _xDecodeOpEn_ZO},
#endif
    {0x38, 2, OpEn_MR, OpSz_8, ImmSz_0, _DecodeOpEn_MR},
    {0x39, 2, OpEn_MR, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_MR},
    {0x3A, 2, OpEn_RM, OpSz_8, ImmSz_0, _DecodeOpEn_RM},
    {0x3B, 2, OpEn_RM, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_RM},
    {0x3C, 1, OpEn_I, OpSz_0, ImmSz_8, _DecodeOpEn_I},
    {0x3D, 1, OpEn_I, OpSz_16 | OpSz_32, ImmSz_16 | ImmSz_32, _DecodeOpEn_I},
    {0x3E, _xDecodeSegPrefix},
#if defined(_M_X64) || defined(__x86_64__)
    {0x3F, _xInvalidOpHanlder},
#else
    {0x3F, _xDecodeOpEn_ZO},
#endif
#if defined(_M_X64) || defined(__x86_64__) // For REX Prefix
    {0x40, _xDecodeREXPrefix},
    {0x41, _xDecodeREXPrefix},
    {0x42, _xDecodeREXPrefix},
    {0x43, _xDecodeREXPrefix},
    {0x44, _xDecodeREXPrefix},
    {0x45, _xDecodeREXPrefix},
    {0x46, _xDecodeREXPrefix},
    {0x47, _xDecodeREXPrefix},
    {0x48, _xDecodeREXPrefix},
    {0x49, _xDecodeREXPrefix},
    {0x4A, _xDecodeREXPrefix},
    {0x4B, _xDecodeREXPrefix},
    {0x4C, _xDecodeREXPrefix},
    {0x4D, _xDecodeREXPrefix},
    {0x4E, _xDecodeREXPrefix},
    {0x4F, _xDecodeREXPrefix},
#else
    {0x40, _xDecodeOpEn_O},
    {0x41, _xDecodeOpEn_O},
    {0x42, _xDecodeOpEn_O},
    {0x43, _xDecodeOpEn_O},
    {0x44, _xDecodeOpEn_O},
    {0x45, _xDecodeOpEn_O},
    {0x46, _xDecodeOpEn_O},
    {0x47, _xDecodeOpEn_O},
    {0x48, _xDecodeOpEn_O},
    {0x49, _xDecodeOpEn_O},
    {0x4A, _xDecodeOpEn_O},
    {0x4B, _xDecodeOpEn_O},
    {0x4C, _xDecodeOpEn_O},
    {0x4D, _xDecodeOpEn_O},
    {0x4E, _xDecodeOpEn_O},
    {0x4F, _xDecodeOpEn_O},
#endif
    {0x50, _xDecodeOpEn_O},
    {0x51, _xDecodeOpEn_O},
    {0x52, _xDecodeOpEn_O},
    {0x53, _xDecodeOpEn_O},
    {0x54, _xDecodeOpEn_O},
    {0x55, _xDecodeOpEn_O},
    {0x56, _xDecodeOpEn_O},
    {0x57, _xDecodeOpEn_O},
    {0x58, _xDecodeOpEn_O},
    {0x59, _xDecodeOpEn_O},
    {0x5A, _xDecodeOpEn_O},
    {0x5B, _xDecodeOpEn_O},
    {0x5C, _xDecodeOpEn_O},
    {0x5D, _xDecodeOpEn_O},
    {0x5E, _xDecodeOpEn_O},
    {0x5F, _xDecodeOpEn_O},
#if defined(_M_X64) || defined(__x86_64__)
    {0x60, _xInvalidOpHanlder},
    {0x61, _xInvalidOpHanlder},
    {0x62, _xInvalidOpHanlder},
#else
    {0x60, _xDecodeOpEn_ZO},
    {0x61, _xDecodeOpEn_ZO},
    {0x62, 2, OpEn_RM, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_RM},
#endif
    {0x63, 2, OpEn_RM, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_RM},
    {0x64, _xDecodeSegPrefix},
    {0x65, _xDecodeSegPrefix},
    {0x66, _xDecodePrefix_66},
    {0x67, _xDecodePrefix_67},
    {0x68, 1, OpEn_I, OpSz_16 | OpSz_32, ImmSz_16 | ImmSz_32, _DecodeOpEn_I},
    {0x69, 2, OpEn_RMI, OpSz_16 | OpSz_32, ImmSz_16 | ImmSz_32, _DecodeOpEn_RMI},
    {0x6A, 1, OpEn_I, OpSz_0, ImmSz_8, _DecodeOpEn_I},
    {0x6B, 1, OpEn_RMI, OpSz_16 | OpSz_32, ImmSz_8, _DecodeOpEn_RMI},
    {0x6C, _xDecodeOpEn_ZO},
    {0x6D, _xDecodeOpEn_ZO},
    {0x6E, _xDecodeOpEn_ZO},
    {0x6F, _xDecodeOpEn_ZO},
    {0x70, 1, OpEn_D, OpSz_0, ImmSz_8, _DecodeOpEn_D},
    {0x71, 1, OpEn_D, OpSz_0, ImmSz_8, _DecodeOpEn_D},
    {0x72, 1, OpEn_D, OpSz_0, ImmSz_8, _DecodeOpEn_D},
    {0x73, 1, OpEn_D, OpSz_0, ImmSz_8, _DecodeOpEn_D},
    {0x74, 1, OpEn_D, OpSz_0, ImmSz_8, _DecodeOpEn_D},
    {0x75, 1, OpEn_D, OpSz_0, ImmSz_8, _DecodeOpEn_D},
    {0x76, 1, OpEn_D, OpSz_0, ImmSz_8, _DecodeOpEn_D},
    {0x77, 1, OpEn_D, OpSz_0, ImmSz_8, _DecodeOpEn_D},
    {0x78, 1, OpEn_D, OpSz_0, ImmSz_8, _DecodeOpEn_D},
    {0x79, 1, OpEn_D, OpSz_0, ImmSz_8, _DecodeOpEn_D},
    {0x7A, 1, OpEn_D, OpSz_0, ImmSz_8, _DecodeOpEn_D},
    {0x7B, 1, OpEn_D, OpSz_0, ImmSz_8, _DecodeOpEn_D},
    {0x7C, 1, OpEn_D, OpSz_0, ImmSz_8, _DecodeOpEn_D},
    {0x7D, 1, OpEn_D, OpSz_0, ImmSz_8, _DecodeOpEn_D},
    {0x7E, 1, OpEn_D, OpSz_0, ImmSz_8, _DecodeOpEn_D},
    {0x7F, 1, OpEn_D, OpSz_0, ImmSz_8, _DecodeOpEn_D},
    {0x80, 2, OpEn_MI, OpSz_8, ImmSz_8, _DecodeOpEn_MI},
    {0x81, 2, OpEn_MI, OpSz_16 | OpSz_32, ImmSz_16 | ImmSz_32, _DecodeOpEn_MI},
#if defined(_M_X64) || defined(__x86_64__)
    {0x82, _xInvalidOpHanlder},
#else
    {0x82, _xUnknownOpHanlder},
#endif
    {0x83, 2, OpEn_MI, OpSz_16 | OpSz_32, ImmSz_8, _DecodeOpEn_MI},
    {0x84, 2, OpEn_MR, OpSz_8, ImmSz_0, _DecodeOpEn_MR},
    {0x85, 2, OpEn_MR, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_MR},
    {0x86, 2, OpEn_RM, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_RM},
    {0x87, 2, OpEn_RM, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_RM},
    {0x88, 2, OpEn_MR, OpSz_8, ImmSz_0, _DecodeOpEn_MR},
    {0x89, 2, OpEn_MR, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_MR},
    {0x8A, 2, OpEn_RM, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_RM},
    {0x8B, 2, OpEn_RM, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_RM},
    {0x8C, 2, OpEn_MR, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_MR},
    {0x8D, 2, OpEn_RM, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_RM},
    {0x8E, 2, OpEn_RM, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_RM},
    {0x8F, 2, OpEn_M, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_M},
    {0x90, _xDecodeOpEn_ZO},
    {0x91, _xInvalidOpHanlder},
    {0x92, _xInvalidOpHanlder},
    {0x93, _xInvalidOpHanlder},
    {0x94, _xInvalidOpHanlder},
    {0x95, _xInvalidOpHanlder},
    {0x96, _xInvalidOpHanlder},
    {0x97, _xInvalidOpHanlder},
    {0x98, _xDecodeOpEn_ZO},
    {0x99, _xDecodeOpEn_ZO},
#if defined(_M_X64) || defined(__x86_64__)
    {0x9A, _xInvalidOpHanlder},
#else
    {0x9A, _xDecodeOpEn_ZO},
#endif
    {0x9B, _xDecodeOpEn_ZO},
    {0x9C, _xDecodeOpEn_ZO},
    {0x9D, _xDecodeOpEn_ZO},
    {0x9E, _xDecodeOpEn_ZO},
    {0x9F, _xDecodeOpEn_ZO},
    {0xA0, _xUnknownOpHanlder},
    {0xA1, _xUnknownOpHanlder},
    {0xA2, _xUnknownOpHanlder},
    {0xA3, _xUnknownOpHanlder},
    {0xA4, _xDecodeOpEn_ZO},
    {0xA5, _xDecodeOpEn_ZO},
    {0xA6, _xDecodeOpEn_ZO},
    {0xA7, _xDecodeOpEn_ZO},
    {0xA8, 1, OpEn_I, OpSz_0, ImmSz_8, _DecodeOpEn_I},
    {0xA9, 1, OpEn_I, OpSz_16 | OpSz_32, ImmSz_16 | ImmSz_32, _DecodeOpEn_I},
    {0xAA, _xDecodeOpEn_ZO},
    {0xAB, _xDecodeOpEn_ZO},
    {0xAC, _xDecodeOpEn_ZO},
    {0xAD, _xDecodeOpEn_ZO},
    {0xAE, _xDecodeOpEn_ZO},
    {0xAF, _xDecodeOpEn_ZO},
#undef SAME_ITEM_LAZY
#define SAME_ITEM_LAZY 1, OpEn_OI, OpSz_0, ImmSz_8, _DecodeOpEn_OI
    {0xB0, SAME_ITEM_LAZY},
    {0xB1, SAME_ITEM_LAZY},
    {0xB2, SAME_ITEM_LAZY},
    {0xB3, SAME_ITEM_LAZY},
    {0xB4, SAME_ITEM_LAZY},
    {0xB5, SAME_ITEM_LAZY},
    {0xB6, SAME_ITEM_LAZY},
    {0xB7, SAME_ITEM_LAZY},
#undef SAME_ITEM_LAZY
#define SAME_ITEM_LAZY 1, OpEn_OI, OpSz_16 | OpSz_32, ImmSz_16 | ImmSz_32, _DecodeOpEn_OI
    {0xB8, SAME_ITEM_LAZY},
    {0xB9, SAME_ITEM_LAZY},
    {0xBA, SAME_ITEM_LAZY},
    {0xBB, SAME_ITEM_LAZY},
    {0xBC, SAME_ITEM_LAZY},
    {0xBD, SAME_ITEM_LAZY},
    {0xBE, SAME_ITEM_LAZY},
    {0xBF, SAME_ITEM_LAZY},
    {0xC0, 2, OpEn_MI, OpSz_8, ImmSz_8, _DecodeOpEn_MI},
    {0xC1, 2, OpEn_MI, OpSz_16 | OpSz_32, ImmSz_16 | ImmSz_32, _DecodeOpEn_MI},
    {0xC2, 1, OpEn_I, OpSz_0, ImmSz_16, _DecodeOpEn_I},
    {0xC3, _xDecodeOpEn_ZO},
    {0xC4, _xInvalidOpHanlder},
    {0xC5, _xInvalidOpHanlder},
    {0xC6, _xUnknownOpHanlder},
    {0xC7, 2, OpEn_MI, OpSz_16 | OpSz_32, ImmSz_16 | ImmSz_32, _DecodeOpEn_MI},
    {0xC8, _xDecodeOpC8},
    {0xC9, _xDecodeOpEn_ZO},
    {0xCA, 1, OpEn_I, OpSz_0, ImmSz_16, _DecodeOpEn_I},
    {0xCB, _xDecodeOpEn_ZO},
    {0xCC, _xDecodeOpEn_ZO},
    {0xCD, 1, OpEn_I, OpSz_0, ImmSz_8, _DecodeOpEn_I},
#if defined(_M_X64) || defined(__x86_64__)
    {0xCE, _xInvalidOpHanlder},
#else
    {0xCE, _xDecodeOpEn_ZO},
#endif
    {0xCF, _xDecodeOpEn_ZO},
    {0xD0, 1, OpEn_M1, OpSz_8, ImmSz_0, _DecodeOpEn_M1},
    {0xD1, 1, OpEn_M1, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_M1},
    {0xD2, 1, OpEn_MC, OpSz_8, ImmSz_0, _DecodeOpEn_MC},
    {0xD3, 1, OpEn_MC, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_MC},
#if defined(_M_X64) || defined(__x86_64__)
    {0xD4, _xInvalidOpHanlder},
    {0xD5, _xInvalidOpHanlder},
#else
    {0xD4, 1, OpEn_I, OpSz_0, ImmSz_8, _DecodeOpEn_I},
    {0xD5, 1, OpEn_I, OpSz_0, ImmSz_8, _DecodeOpEn_I},
#endif
    {0xD6, _xInvalidOpHanlder},
    {0xD7, _xDecodeOpEn_ZO},
    {0xD8, _xUnknownOpHanlder},
    {0xD9, _xUnknownOpHanlder},
    {0xDA, _xUnknownOpHanlder},
    {0xDB, _xUnknownOpHanlder},
    {0xDC, _xUnknownOpHanlder},
    {0xDD, _xUnknownOpHanlder},
    {0xDE, _xUnknownOpHanlder},
    {0xDF, _xUnknownOpHanlder},
    {0xE0, 1, OpEn_D, OpSz_0, ImmSz_8, _DecodeOpEn_D},
    {0xE1, 1, OpEn_D, OpSz_0, ImmSz_8, _DecodeOpEn_D},
    {0xE2, 1, OpEn_D, OpSz_0, ImmSz_8, _DecodeOpEn_D},
    {0xE3, 1, OpEn_D, OpSz_0, ImmSz_8, _DecodeOpEn_D},
    {0xE4, 1, OpEn_I, OpSz_0, ImmSz_8, _DecodeOpEn_I},
    {0xE5, 1, OpEn_I, OpSz_0, ImmSz_8, _DecodeOpEn_I},
    {0xE6, 1, OpEn_I, OpSz_0, ImmSz_8, _DecodeOpEn_I},
    {0xE7, 1, OpEn_I, OpSz_0, ImmSz_8, _DecodeOpEn_I},
    {0xE8, 1, OpEn_I, OpSz_16 | OpSz_32, ImmSz_16 | ImmSz_32, _DecodeOpEn_I},
    {0xE9, 1, OpEn_I, OpSz_16 | OpSz_32, ImmSz_16 | ImmSz_32, _DecodeOpEn_I},
#if defined(_M_X64) || defined(__x86_64__)
    {0xEA, _xInvalidOpHanlder},
#else
    {0xEA, _xUnknownOpHanlder},
#endif
    {0xEB, 1, OpEn_I, OpSz_0, ImmSz_8, _DecodeOpEn_I},
    {0xEC, _xDecodeOpEn_ZO},
    {0xED, _xDecodeOpEn_ZO},
    {0xEE, _xDecodeOpEn_ZO},
    {0xEF, _xDecodeOpEn_ZO},
    {0xF0, _xDecodePrefix},
    {0xF1, _xDecodeOpEn_ZO},
    {0xF2, _xDecodeOpEn_ZO},
#ifdef DETOURS_X86
    {0xF3, _CopyF3},
#else
    {0xF3, _xDecodeOpEn_ZO},
#endif
    {0xF4, _xDecodeOpEn_ZO},
    {0xF5, _xDecodeOpEn_ZO},
    {0xF6, 2, OpEn_MI, OpSz_8, ImmSz_8, _DecodeOpEn_MI},
    {0xF7, 2, OpEn_MI, OpSz_16 | OpSz_32, ImmSz_16 | ImmSz_32, _DecodeOpEn_MI},
    {0xF8, _xDecodeOpEn_ZO},
    {0xF9, _xDecodeOpEn_ZO},
    {0xFA, _xDecodeOpEn_ZO},
    {0xFB, _xDecodeOpEn_ZO},
    {0xFC, _xDecodeOpEn_ZO},
    {0xFD, _xDecodeOpEn_ZO},
    {0xFE, 2, OpEn_M, OpSz_8, ImmSz_0, _DecodeOpEn_M},
    {0xFF, 2, OpEn_M, OpSz_16 | OpSz_32, ImmSz_0, _DecodeOpEn_M},
    {0, 0, 0, 0, 0}};

#endif