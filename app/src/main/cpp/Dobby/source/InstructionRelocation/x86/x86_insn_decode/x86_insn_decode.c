#include "platform_macro.h"
#if defined(TARGET_ARCH_IA32) || defined(TARGET_ARCH_X64)

#include "x86_insn_decode.h"

#include "logging/logging.h"

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

#if 0
/* Build an encoding specification from scratch. */
#define SPEC_MAKE(op, opr1, opr2, opr3, opr4)                                                                          \
  ((uint64_t)(uint16_t)(int16_t)(op) | ((uint64_t)(opr1) << 16) | ((uint64_t)(opr2) << 24) |                           \
   ((uint64_t)(opr3) << 32) | ((uint64_t)(opr4) << 40))

/* Get the operation in an encoding specification. */
#define SPEC_INSN(spec) ((int16_t)((spec)&0xffff))

/* Get the given operand (zero-based) in an encoding specification. */
#define SPEC_OPERAND(spec, i) ((uint8_t)(((spec) >> (16 + (i)*8)) & 0xff))

/* Get the operands part of an encoding specification. */
#define SPEC_OPERANDS(spec) ((spec)&0xffffffffffff0000ULL)

/* Merges two encoding specifications. */
#define SPEC_MERGE(spec1, spec2) ((spec1) | (spec2))

#define OP4(insn, oper1, oper2, oper3, oper4) SPEC_MAKE(I_##insn, O_##oper1, O_##oper2, O_##oper3, O_##oper4)
#define OP3(insn, oper1, oper2, oper3) OP4(insn, oper1, oper2, oper3, NONE)
#define OP2(insn, oper1, oper2) OP3(insn, oper1, oper2, NONE)
#define OP1(insn, oper1) OP2(insn, oper1, NONE)
#define OP0(insn) OP1(insn, NONE)
#define OP_EMPTY OP0(NONE)
#define OP_EMPTY_4 OP_EMPTY, OP_EMPTY, OP_EMPTY, OP_EMPTY
#define OP_EMPTY_8 OP_EMPTY_4, OP_EMPTY_4
#endif

#define op3_flag(x, f, o0, o1, o2)                                                                                     \
  {                                                                                                                    \
    .name = #x, .flags = (f), .operands[0] = {.data = #o0}, .operands[1] = {.data = #o1},                              \
    .operands[2] = {.data = #o2},                                                                                      \
  }
#define op2_flag(x, f, o0, o1) op3_flag(x, f, o0, o1, __)
#define op1_flag(x, f, o0) op2_flag(x, f, o0, __)
#define op0_flag(x, f) op1_flag(x, f, __)

#define op3f op3_flag
#define op2f op2_flag
#define op1f op1_flag
#define op0f op0_flag

#define op3(x, o0, o1, o2) op3f(x, 0, o0, o1, o2)
#define op2(x, o0, o1) op2f(x, 0, o0, o1)
#define op1(x, o0) op1f(x, 0, o0)
#define op0(x) op0f(x, 0)

/* Opcode extension in modrm byte reg field. */
#define foreach_x86_insn_modrm_reg_group                                                                               \
  _(1) _(1a) _(2) _(3) _(4) _(5) _(6) _(7) _(8) _(9) _(10) _(11) _(12) _(13) _(14) _(15) _(16) _(p)
#define foreach_x86_insn_sse_group                                                                                     \
  _(10) _(28) _(50) _(58) _(60) _(68) _(70) _(78) _(c0) _(d0) _(d8) _(e0) _(e8) _(f0) _(f8)
enum {
  X86_INSN_GROUP_START = 0,

#define _(x) X86_INSN_MODRM_REG_GROUP_##x,
  foreach_x86_insn_modrm_reg_group
#undef _

      X86_INSN_SSE_GROUP_START = 19,
#define _(x) X86_INSN_SSE_GROUP_##x,
  foreach_x86_insn_sse_group
#undef _

      X86_INSN_GROUP_END = 35
};

#define X86_INSN_GROUP_END_MASK ((1 << 6) - 1)
#define X86_INSN_FLAG_SET_GROUP(n) ((n) << 5)
#define X86_INSN_FLAG_GET_GROUP(f) (((f) >> 5) & X86_INSN_GROUP_END_MASK)

enum {
#define _(x) X86_INSN_FLAG_MODRM_REG_GROUP_##x = X86_INSN_FLAG_SET_GROUP(X86_INSN_MODRM_REG_GROUP_##x),
  foreach_x86_insn_modrm_reg_group
#undef _

#define _(x) X86_INSN_FLAG_SSE_GROUP_##x = X86_INSN_FLAG_SET_GROUP(X86_INSN_SSE_GROUP_##x),
      foreach_x86_insn_sse_group
#undef _
};

// clang-format off

#define foreach_x86_operand_combine(x, op1_type, op2_type)  op2(x, Eb, Gb), op2(x, Ev, Gv), op2(x, Gb, Eb), op2(x, Gv, Ev), op2(x, AL, Ib), op2(x, AX, Iz)

#define foreach_x86_gp_reg _(AX) _(CX) _(DX) _(BX) _(SP) _(BP) _(SI) _(DI)

#define foreach_x86_condition _(o) _(no) _(b) _(nb) _(z) _(nz) _(be) _(nbe) _(s) _(ns) _(p) _(np) _(l) _(nl) _(le) _(nle)

// clang-format on

#include "./x86_opcode_one_byte.c"
#include "./x86_opcode_two_byte.c"

typedef struct {
  x86_insn_spec_t insns[8];
} x86_insn_group8_t;

#include "./x86_opcode_modrm_reg_group.c"
#include "./x86_opcode_sse_group.c"

#include "./x86_insn_reader.c"

static x86_insn_prefix_t x86_insn_decode_prefix(x86_insn_reader_t *rd, x86_insn_decode_t *insn, x86_options_t *conf) {
  /* Decode each byte until the byte is not a prefix or is an REX prefix,
   * because an REX prefix is required to immediately preceed the opcode.
   */
  x86_insn_prefix_t insn_prefix = 0;
  for (;;) {
    uint8_t c = peek_byte(rd);
    x86_insn_prefix_t t = 0;

    /* Check for REX prefix if we're in 64-bit mode. */
    if (conf->mode == 64) {
      if (c >= 0x40 && c <= 0x4f) {
        uint8_t rex = read_byte(rd);

        if (REX_W(rex)) {
          insn->flags |= X86_INSN_DECODE_FLAG_OPERAND_SIZE_64;
        }

        insn->rex = rex;

        break;
      }
    }

    /* Check for legacy prefixes. */
    switch (c) {
    case 0xF0:
      t = INSN_PREFIX_LOCK;
      break;
    case 0xF2:
      t = INSN_PREFIX_REPNE;
      break;
    case 0xF3:
      t = INSN_PREFIX_REPE;
      break;
    case 0x2E:
      t = INSN_PREFIX_CS;
      break;
    case 0x36:
      t = INSN_PREFIX_SS;
      break;
    case 0x3E:
      t = INSN_PREFIX_DS;
      break;
    case 0x26:
      t = INSN_PREFIX_ES;
      break;
    case 0x64:
      t = INSN_PREFIX_FS;
      break;
    case 0x65:
      t = INSN_PREFIX_GS;
      break;
    case 0x66:
      t = INSN_PREFIX_OPERAND_SIZE;
      break;
    case 0x67:
      t = INSN_PREFIX_ADDRESS_SIZE;
      break;
    }
    if (t == 0)
      break;

    /* Consume 1 byte. */
    read_byte(rd);
    insn_prefix |= t;
  }

  return insn_prefix;
}

int x86_insn_has_modrm_byte(x86_insn_spec_t *insn) {
  int i;
  for (i = 0; i < sizeof(insn->operands) / sizeof(x86_insn_operand_spec_t); i++)
    switch (insn->operands[i].code) {
    case 'G':
    case 'E':
    case 'M':
    case 'R':
      return 1;
    }
  return 0;
}

int x86_insn_immediate_type(x86_insn_spec_t *insn) {
  int i;
  for (i = 0; i < sizeof(insn->operands); i++) {
    switch (insn->operands[i].code) {
    case 'J':
    case 'I':
    case 'O':
      return insn->operands[i].type;
    }
  }
  return 0;
}

int x86_insn_has_immediate(x86_insn_spec_t *insn) {
  int i;
  for (i = 0; i < sizeof(insn->operands) / sizeof(x86_insn_operand_spec_t); i++) {
    switch (insn->operands[i].code) {
    case 'J':
    case 'I':
    case 'O':
      return 1;
    }
  }
  return 0;
}

static uint8_t *x86_insn_decode_number(x86_insn_reader_t *rd, uint8_t number_bits, int64_t *out_number) {
  int64_t disp = 0;
  switch (number_bits) {
  case 64:
    disp = read_uint64(rd);
    break;
  case 32:
    disp = read_uint32(rd);
    break;
  case 16:
    disp = read_uint16(rd);
    break;
  case 8:
    disp = read_uint8(rd);
    break;
  default:
    UNREACHABLE();
  }

  *out_number = disp;
  return NULL;
}

void x86_insn_decode_modrm_sib(x86_insn_reader_t *rd, x86_insn_decode_t *insn, x86_options_t *conf) {
  uint8_t mod, rm, reg;

  x86_insn_modrm_t modrm;
  modrm.byte = read_byte(rd);
  insn->modrm = modrm;

  mod = modrm.mode;
  rm = (uint8_t)((REX_B(insn->rex) << 3) | modrm.rm);
  reg = (uint8_t)((REX_R(insn->rex) << 3) | modrm.reg);

  x86_insn_operand_t *reg_op = &insn->operands[0];
  x86_insn_operand_t *mem_op = &insn->operands[1];

  reg_op->reg = reg;

  if (mod == 3) {
    mem_op->reg = rm;
    return;
  }

  uint8_t disp_bits = -1;

  insn->flags |= X86_INSN_DECODE_FLAG_IS_ADDRESS;

  uint8_t effective_address_bits;
  if (conf->mode == 64)
    effective_address_bits = (insn->prefix & INSN_PREFIX_ADDRESS_SIZE) ? 32 : 64;
  else if (conf->mode == 32)
    effective_address_bits = (insn->prefix & INSN_PREFIX_ADDRESS_SIZE) ? 16 : 32;
  else {
    FATAL("16-bit address mode not supported");
  }

  if (effective_address_bits == 32 || effective_address_bits == 64) {
    mem_op->mem.base = rm;

    insn->flags |= X86_INSN_DECODE_FLAG_HAS_BASE;

    if (mod == 0 && (rm & 7) == 5) {
      insn->flags = X86_INSN_DECODE_FLAG_IP_RELATIVE;
      mem_op->mem.base = RIP;
      disp_bits = 32;
    } else if (mod == 0) {
      disp_bits = 0;
    } else if (mod == 1) {
      disp_bits = 8;
    } else if (mod == 2) {
      disp_bits = 32;
    } else {
      disp_bits = 0;
    }

    uint8_t has_sib = 0;
    if ((rm & 7) == 4) {
      ASSERT(modrm.rm == (rm & 7));
      has_sib = 1;
    }

    if (has_sib) {
      x86_insn_sib_t sib = {0};
      sib.byte = read_byte(rd);
      insn->sib = sib;

      uint8_t base = (uint8_t)(sib.base | (REX_B(insn->rex) << 3));
      uint8_t index = (uint8_t)(sib.index | (REX_X(insn->rex) << 3));
      uint8_t scale = (uint8_t)(1 << sib.log2_scale);

      insn->flags |= X86_INSN_DECODE_FLAG_HAS_BASE;

      if (sib.index != X86_INSN_GP_REG_SP) {
        insn->flags |= X86_INSN_DECODE_FLAG_HAS_INDEX;
      }

      insn->operands[1].mem.base = base;
      insn->operands[1].mem.index = index;
      insn->operands[1].mem.scale = scale;

      if (sib.index == X86_INSN_GP_REG_SP) {
        insn->operands[1].mem.index = RNone;
        insn->operands[1].mem.scale = 0;
      }

      // for 64 bit
      if (effective_address_bits == 64) {
        if (mem_op->mem.base == RBP || mem_op->mem.base == R13) {
          if (mod == 0) {
            mem_op->mem.base = RNone;
          }
          if (mod == 1) {
            disp_bits = 8;
          } else {
            disp_bits = 32;
          }
        }

        if (sib.index != X86_INSN_GP_REG_SP) {
          insn->flags |= X86_INSN_DECODE_FLAG_HAS_INDEX;
        }
      }

      // for 32 bit
      if (effective_address_bits == 32) {
        if (mem_op->mem.base == RBP) {
          if (mod == 0) {
            mem_op->mem.base = RNone;
          }
          if (mod == 1) {
            disp_bits = 8;
          } else {
            disp_bits = 32;
          }
        }
      }
    }
  }

  // for 16 bit
  if (effective_address_bits == 16) {
    switch (modrm.mode) {
    case 0:
      if (modrm.rm == 6) {
        /* [disp16] */
        disp_bits = 16;
        break;
      }
      /* fall through */
    case 1:
    case 2:
      switch (modrm.rm) {
      case 0: /* [bx + si/di] */
      case 1:
        mem_op->mem.base = X86_INSN_GP_REG_BX;
        mem_op->mem.index = X86_INSN_GP_REG_SI + (modrm.rm & 1);
        insn->flags |= X86_INSN_DECODE_FLAG_HAS_BASE | X86_INSN_DECODE_FLAG_HAS_INDEX;
        break;

      case 2: /* [bp + si/di] */
      case 3:
        mem_op->mem.base = X86_INSN_GP_REG_BP;
        mem_op->mem.index = X86_INSN_GP_REG_SI + (modrm.rm & 1);
        insn->flags |= X86_INSN_DECODE_FLAG_HAS_BASE | X86_INSN_DECODE_FLAG_HAS_INDEX;
        break;

      case 4: /* [si/di] */
      case 5:
        mem_op->mem.base = X86_INSN_GP_REG_SI + (modrm.rm & 1);
        insn->flags |= X86_INSN_DECODE_FLAG_HAS_BASE;
        break;

      case 6: /* [bp + disp] */
        mem_op->mem.base = X86_INSN_GP_REG_BP;
        insn->flags |= X86_INSN_DECODE_FLAG_HAS_BASE;
        break;

      case 7: /* [bx + disp] */
        mem_op->mem.base = X86_INSN_GP_REG_BX;
        insn->flags |= X86_INSN_DECODE_FLAG_HAS_BASE;
        break;
      }

      if (modrm.mode != 0)
        disp_bits = modrm.mode == 1 ? 8 : 16;
      break;
    }
  }

  if (disp_bits != 0) {
    // update displacement offset
    insn->displacement_offset = (uint8_t)reader_offset(rd);

    int64_t disp;
    x86_insn_decode_number(rd, disp_bits, &disp);
    mem_op->mem.disp = disp;
  }
}

/* Decodes the opcode of an instruction and returns its encoding
 * specification.
 */
static void x86_insn_decode_opcode(x86_insn_reader_t *rd, x86_insn_decode_t *insn, x86_options_t *conf) {
  uint8_t opcode = read_byte(rd);

  x86_insn_spec_t insn_spec;
  if (opcode == 0x0f) {
    opcode = read_byte(rd);
    insn_spec = x86_opcode_map_two_byte[opcode];
  } else {
    insn_spec = x86_opcode_map_one_byte[opcode];
  }

  // check sse group
  if (X86_INSN_FLAG_GET_GROUP(insn_spec.flags) > X86_INSN_SSE_GROUP_START) {
    UNIMPLEMENTED();
  }

  if (X86_INSN_FLAG_GET_GROUP(insn_spec.flags) > X86_INSN_GROUP_START &&
      X86_INSN_FLAG_GET_GROUP(insn_spec.flags) < X86_INSN_SSE_GROUP_START) {
    // get group index
    int group_ndx = X86_INSN_FLAG_GET_GROUP(insn_spec.flags);

    // get gp insn index in group
    x86_insn_modrm_t modrm;
    modrm.byte = peek_byte(rd);
    int insn_ndx = modrm.reg;

    // get insn in group
    x86_insn_spec_t *group_insn = NULL;
    group_insn = &x86_insn_modrm_reg_groups[group_ndx].insns[insn_ndx];

    // update the insn spec
    insn_spec.name = group_insn->name;
    insn_spec.flags = group_insn->flags;
  }

  insn->primary_opcode = opcode;
  insn->insn_spec = insn_spec;
}

uint8_t x86_insn_imm_bits(x86_insn_spec_t *insn, uint8_t operand_bits) {
  uint8_t imm_bits = 0;
  switch (x86_insn_immediate_type(insn)) {
  case 'b':
    imm_bits = 8;
    break;
  case 'w':
    imm_bits = 16;
    break;
  case 'd':
    imm_bits = 32;
    break;
  case 'q':
    imm_bits = 64;
    break;

  case 'z':
    imm_bits = operand_bits;
    if (imm_bits == 64)
      imm_bits = 32;
    break;

  case 'v':
    imm_bits = operand_bits;
    break;

  default:
    imm_bits = 0;
    break;
  }

  return imm_bits;
}

void x86_insn_decode_immediate(x86_insn_reader_t *rd, x86_insn_decode_t *insn, x86_options_t *conf) {
  uint8_t effective_operand_bits;
  if (conf->mode == 64 || conf->mode == 32) {
    effective_operand_bits = (insn->prefix & INSN_PREFIX_OPERAND_SIZE) ? 16 : 32;
  }
  effective_operand_bits = (insn->prefix & INSN_PREFIX_OPERAND_SIZE) ? 16 : 32;

  if (insn->flags & X86_INSN_DECODE_FLAG_OPERAND_SIZE_64)
    effective_operand_bits = 64;

  if (conf->mode == 64 && insn->insn_spec.flags & X86_INSN_SPEC_DEFAULT_64_BIT)
    effective_operand_bits = 64;

  int64_t immediate = 0;
  uint8_t imm_bits = x86_insn_imm_bits(&insn->insn_spec, effective_operand_bits);
  if (imm_bits == 0)
    return;

  // update immediate offset
  insn->immediate_offset = (uint8_t)reader_offset(rd);

  x86_insn_decode_number(rd, imm_bits, &immediate);
  insn->immediate = immediate;
}

void x86_insn_decode(x86_insn_decode_t *insn, uint8_t *buffer, x86_options_t *conf) {
  // init reader
  x86_insn_reader_t rd;
  init_reader(&rd, buffer, buffer + 15);

  // decode prefix
  insn->prefix = x86_insn_decode_prefix(&rd, insn, conf);

  // decode insn specp/x in
  x86_insn_decode_opcode(&rd, insn, conf);

  if (x86_insn_has_modrm_byte(&insn->insn_spec)) {
    // decode insn modrm sib (operand register, disp)
    x86_insn_decode_modrm_sib(&rd, insn, conf);
  }

  if (x86_insn_has_immediate(&insn->insn_spec)) {
    // decode insn immeidate
    x86_insn_decode_immediate(&rd, insn, conf);
  }

#if 1
  DLOG(0, "[x86 insn] %s", insn->insn_spec.name);
#endif

  // set insn length
  insn->length = rd.buffer_cursor - rd.buffer;
}

#endif