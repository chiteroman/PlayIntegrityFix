#ifndef X86_INSN_DECODE_H
#define X86_INSN_DECODE_H

#include <stdint.h>
#include "dobby/common.h"

typedef enum {
  X86_INSN_SPEC_DEFAULT_64_BIT = 1 << 0,
} x86_insn_spec_flag_t;

typedef enum {
  X86_INSN_DECODE_FLAG_HAS_BASE = 1 << 0,

  X86_INSN_DECODE_FLAG_HAS_INDEX = 1 << 1,

  X86_INSN_DECODE_FLAG_IS_ADDRESS = 1 << 2,

  X86_INSN_DECODE_FLAG_IP_RELATIVE = 1 << 3,

  X86_INSN_DECODE_FLAG_OPERAND_SIZE_64 = 1 << 4,
} x86_insn_decode_flag_t;

typedef enum {
  INSN_PREFIX_NONE = 0,

  /* Group 1: lock and repeat prefixes */
  INSN_PREFIX_GROUP1 = 0x07,
  INSN_PREFIX_LOCK = 0x01,  /* F0 */
  INSN_PREFIX_REPNZ = 0x02, /* F2 */
  INSN_PREFIX_REPNE = INSN_PREFIX_REPNZ,
  INSN_PREFIX_REP = 0x04, /* F3 */
  INSN_PREFIX_REPZ = INSN_PREFIX_REP,
  INSN_PREFIX_REPE = INSN_PREFIX_REPZ,

  /* Group 2: segment override or branch hints */
  INSN_PREFIX_GROUP2 = 0x01f8,
  INSN_PREFIX_ES = 0x0008,                       /* 26 */
  INSN_PREFIX_CS = 0x0010,                       /* 2E */
  INSN_PREFIX_SS = 0x0020,                       /* 36 */
  INSN_PREFIX_DS = 0x0040,                       /* 3E */
  INSN_PREFIX_FS = 0x0080,                       /* 64 */
  INSN_PREFIX_GS = 0x0100,                       /* 65 */
  INSN_PREFIX_BRANCH_TAKEN = INSN_PREFIX_CS,     /* 2E */
  INSN_PREFIX_BRANCH_NOT_TAKEN = INSN_PREFIX_DS, /* 3E */

  /* Group 3: operand-size override */
  INSN_PREFIX_OPERAND_SIZE = 0x0200, /* 66 */

  /* Group 4: address-size override */
  INSN_PREFIX_ADDRESS_SIZE = 0x0400 /* 67 */
} x86_insn_prefix_t;

typedef union {
  struct {
    uint8_t code;
    uint8_t type;
  };
  uint8_t data[2];
} x86_insn_operand_spec_t;

typedef struct {
  // insn name
  char *name;

  // insn max 3 operands
  x86_insn_operand_spec_t operands[3];

  // insn flag
  uint16_t flags;
#define X86_INSN_FLAG_SET_SSE_GROUP(n) ((n) << 5)
#define X86_INSN_FLAG_GET_SSE_GROUP(f) (((f) >> 5) & 0x1f)
#define X86_INSN_FLAG_SET_MODRM_REG_GROUP(n) (((n)&0x3f) << 10)
#define X86_INSN_FLAG_GET_MODRM_REG_GROUP(f) (((f) >> 10) & 0x3f)
} x86_insn_spec_t;

#define foreach_x86_gp_register _(AX) _(CX) _(DX) _(BX) _(SP) _(BP) _(SI) _(DI)

typedef enum {
#define _(r) X86_INSN_GP_REG_##r,
  foreach_x86_gp_register
#undef _
} x86_insn_gp_register_t;

typedef enum {
  RNone = 0,
  RAX,
  RBX,
  RCX,
  RDX,
  RDI,
  RSI,
  RBP,
  RSP,
  R8,
  R9,
  R10,
  R11,
  R12,
  R13,
  R14,
  R15,
  RIP
} x86_ia32e_register_t;

typedef union {
  struct {
    uint8_t rm : 3;
    uint8_t reg : 3;
    uint8_t mode : 2;
  };
  uint8_t byte;
} x86_insn_modrm_t;

typedef union {
  struct {
    uint8_t base : 3;
    uint8_t index : 3;
    uint8_t log2_scale : 2;
  };
  uint8_t byte;
} x86_insn_sib_t;

typedef struct {
  uint8_t reg;

  struct {
    uint8_t base;
    uint8_t index;
    uint8_t scale;
    uint32_t disp;
  } mem;
} x86_insn_operand_t;

typedef struct x86_insn_decode_t {
  // insn flag
  uint32_t flags;

  // insn length
  uint32_t length;

  // insn displacement offset
  uint8_t displacement_offset;

  // insn immediate offset
  uint8_t immediate_offset;

  // Registers in instruction
  // [0] is modrm reg field
  // [1] is base reg
  // [2] is index reg
  //  union {
  //    struct {
  //      uint8_t modrm_reg;
  //      uint8_t op_base_reg;
  //      uint8_t op_index_reg;
  //    };
  //    uint8_t regs[3];
  //  };

  x86_insn_operand_t operands[3];

  struct { // insn field combine
    // insn prefix
    x86_insn_prefix_t prefix;

    // insn rex
    uint8_t rex;

    // insn primary opcode
    uint8_t primary_opcode;

    // insn modrm
    x86_insn_modrm_t modrm;

    // insn sib
    x86_insn_sib_t sib;

    // insn operand imm
    int64_t immediate;
  };

  // insn pre-spec
  x86_insn_spec_t insn_spec;
} x86_insn_decode_t;

typedef struct x86_options_t {
  int mode; /* 16, 32 or 64 bit */
} x86_options_t;

#ifdef __cplusplus
extern "C" {
#endif

void x86_insn_decode(x86_insn_decode_t *insn, uint8_t *buffer, x86_options_t *conf);

#ifdef __cplusplus
}
#endif

#endif