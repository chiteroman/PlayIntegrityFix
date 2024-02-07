#ifndef X86_OPCODE_DECODE_TABLE_H
#define X86_OPCODE_DECODE_TABLE_H

#ifndef __addr_t_defined
#define __addr_t_defined
typedef unsigned long long addr_t;
#endif

#ifndef __byte_defined
#define __byte_defined
typedef unsigned char byte_t;
#endif

#ifndef __uint_defined
#define __uint_defined
typedef unsigned int uint;
#endif

#ifndef __word_defined
#define __word_defined
typedef short word;
#endif

#ifndef __dword_defined
#define __dword_defined
typedef int dword;
#endif

enum OpcodeType { OpTy_Op1, OpTy_RegInOp1, OpTy_Op1ExtraOp };

struct Instr {
  byte_t prefix;

  byte_t REX;

  union {
    byte_t opcode[3];
    struct {
      byte_t opcode1;
      byte_t opcode2;
      byte_t opcode3;
    };
  };

  union {
    byte_t ModRM;
    struct {
      byte_t Mod : 2;
      byte_t RegOpcode : 3;
      byte_t RM : 3;
    };
  };

  union {
    byte_t SIB;
    struct {
      byte_t base : 2;
      byte_t index : 3;
      byte_t scale : 3;
    };
  };

  byte_t Displacement[4];
  int DisplacementOffset;

  byte_t Immediate[4];
  int ImmediateOffset;
};

// clang-format off
enum OperandSize {
  OpSz_0 = 0,
  OpSz_8=1,
  OpSz_16=2,
  OpSz_32=4,
  OpSz_64=8
};

enum ImmediteSize {
  ImmSz_0      = 0,
  ImmSz_8=1,
  ImmSz_16=2,
  ImmSz_32=4,
  ImmSz_64=8
};

enum InstrFlag {
  kNoFlag = 0,
  kIPRelativeAddress = 1
};
// clang-format on

struct InstrMnemonic {
  uint len;

  int flag;

  OperandSize OperandSz;

  ImmediteSize ImmediteSz;

  struct Instr instr;
};

struct OpcodeDecodeItem {
  unsigned char opcode;

  int FixedSize;

  int OpEn;

  int OperandSz;

  int ImmediteSz;

  void (*DecodeHandler)(InstrMnemonic *, addr_t);
};

// clang-format off
enum OperandEncodingType {
  OpEn_NONE =0,
  OpEn_ZO,
  OpEn_M,
  OpEn_I,
  OpEn_D,
  OpEn_O,
  OpEn_RM,
  OpEn_MR,
  OpEn_MI,
  OpEn_OI,
  OpEn_M1,
  OpEn_MC,
  OpEn_RMI
};

// clang-format on

extern OpcodeDecodeItem OpcodeDecodeTable[257];

void _DecodePrefix(InstrMnemonic *instr, addr_t p);

#endif
