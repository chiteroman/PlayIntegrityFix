#include <stdio.h>
#include <stdint.h>

#include "logging/logging.h"

enum SegmentPrefix {
  kCs = 0x2e,
  kSs = 0x36,
  kDs = 0x3e,
  kEs = 0x26,
  kFs = 0x64,
  kGs = 0x65,
};

bool supports_rex_ = false;

void DecodeInstruction(uint8_t *instr) {
  bool have_prefixes = true;
  uint8_t prefix[4] = {0, 0, 0, 0};

  // decode legacy prefix
  do {
    switch (*instr) {
      // Group 1 - lock and repeat prefixes:
    case 0xF0:
    case 0xF2:
    case 0xF3:
      prefix[0] = *instr;
      break;
      // Group 2 - segment override prefixes:
    case kCs:
    case kSs:
    case kDs:
    case kEs:
    case kFs:
    case kGs:
      prefix[1] = *instr;
      break;
      // Group 3 - operand size override:
    case 0x66:
      prefix[2] = *instr;
      break;
      // Group 4 - address size override:
    case 0x67:
      prefix[3] = *instr;
      break;
    default:
      have_prefixes = false;
      break;
    }
    if (have_prefixes) {
      instr++;
    }
  } while (have_prefixes);

  // x64 rex
  uint8_t rex = (supports_rex_ && (*instr >= 0x40) && (*instr <= 0x4F)) ? *instr : 0;
  if (rex != 0) {
    instr++;
  }

  bool has_modrm = false;
  bool reg_is_opcode = false;

  size_t immediate_bytes = 0;

#define OpEn_MR                                                                                                        \
  do {                                                                                                                 \
    has_modrm = true;                                                                                                  \
  } while (0);                                                                                                         \
  break;

#define OpEn_RM                                                                                                        \
  do {                                                                                                                 \
    has_modrm = true;                                                                                                  \
  } while (0);                                                                                                         \
  break;

#define OpEn_I(immediate_size)                                                                                         \
  do {                                                                                                                 \
    immediate_bytes = immediate_size;                                                                                  \
  } while (0);                                                                                                         \
  break;

#define OpEn_RMI(immediate_size)                                                                                       \
  do {                                                                                                                 \
    immediate_bytes = immediate_size;                                                                                  \
  } while (0);                                                                                                         \
  break;

#define OpEn_O                                                                                                         \
  do {                                                                                                                 \
    reg_is_opcode = true;                                                                                              \
  } while (0);                                                                                                         \
  break;

#define OpEn_D                                                                                                         \
  do {                                                                                                                 \
    reg_is_opcode = true;                                                                                              \
  } while (0);                                                                                                         \
  break;

#define OpEn_ZO                                                                                                        \
  do {                                                                                                                 \
    reg_is_opcode = true;                                                                                              \
  } while (0);                                                                                                         \
  break;

#define Op_Prefix                                                                                                      \
  do {                                                                                                                 \
    reg_is_opcode = true;                                                                                              \
  } while (0);                                                                                                         \
  break;

#define UnImplOpcode                                                                                                   \
  do {                                                                                                                 \
    DLOG(0, "opcode unreachable");                                                                                     \
  } while (0);                                                                                                         \
  break;

  typedef enum {
    MR,
  } OpEnTy;

  // decode opcode
  switch (*instr) {
  case 0x00:
    OpEn_MR;
  case 0x01:
    OpEn_MR;
  case 0x02:
    OpEn_RM;
  case 0x03:
    OpEn_RM;
  case 0x04:
    OpEn_I(8);
  case 0x05:
    OpEn_I(16 | 32);

  case 0x06:
  case 0x07:
    UnImplOpcode;

  case 0x08:
    OpEn_MR;
  case 0x09:
    OpEn_MR;
  case 0x0a:
    OpEn_RM;
  case 0x0b:
    OpEn_RM;
  case 0x0c:
    OpEn_I(8);
  case 0x0d:
    OpEn_I(16 | 32);

  case 0x0e:
  case 0x0f:
    UnImplOpcode;

  case 0x10:
    OpEn_MR;
  case 0x11:
    OpEn_MR;
  case 0x12:
    OpEn_RM;
  case 0x13:
    OpEn_RM;
  case 0x14:
    OpEn_I(8);
  case 0x15:
    OpEn_I(16 | 32);

  case 0x16:
  case 0x17:
    UnImplOpcode;

  case 0x18:
    OpEn_MR;
  case 0x19:
    OpEn_MR;
  case 0x1a:
    OpEn_RM;
  case 0x1b:
    OpEn_RM;
  case 0x1c:
    OpEn_I(8);
  case 0x1d:
    OpEn_I(16 | 32);

  case 0x1e:
  case 0x1f:
    UnImplOpcode;

  case 0x20:
    OpEn_MR;
  case 0x21:
    OpEn_MR;
  case 0x22:
    OpEn_RM;
  case 0x23:
    OpEn_RM;
  case 0x24:
    OpEn_I(8);
  case 0x25:
    OpEn_I(16 | 32);

  case 0x26:
  case 0x27:
    UnImplOpcode;

  case 0x28:
    OpEn_MR;
  case 0x29:
    OpEn_MR;
  case 0x2a:
    OpEn_RM;
  case 0x2b:
    OpEn_RM;
  case 0x2c:
    OpEn_I(8);
  case 0x2d:
    OpEn_I(16 | 32);

  case 0x2e:
  case 0x2f:
    UnImplOpcode;

  case 0x30:
    OpEn_MR;
  case 0x31:
    OpEn_MR;
  case 0x32:
    OpEn_RM;
  case 0x33:
    OpEn_RM;
  case 0x34:
    OpEn_I(8);
  case 0x35:
    OpEn_I(16 | 32);

  case 0x36:
  case 0x37:
    UnImplOpcode;

  case 0x38:
    OpEn_MR;
  case 0x39:
    OpEn_MR;
  case 0x3a:
    OpEn_RM;
  case 0x3b:
    OpEn_RM;
  case 0x3c:
    OpEn_I(8);
  case 0x3d:
    OpEn_I(16 | 32);

  case 0x40:
  case 0x41:
  case 0x42:
  case 0x43:
  case 0x44:
  case 0x45:
  case 0x46:
  case 0x47:
  case 0x48:
  case 0x49:
  case 0x4a:
  case 0x4b:
  case 0x4c:
  case 0x4d:
  case 0x4e:
  case 0x4f:
    UnImplOpcode;

  case 0x50:
  case 0x51:
  case 0x52:
  case 0x53:
  case 0x54:
  case 0x55:
  case 0x56:
  case 0x57:
  case 0x58:
  case 0x59:
  case 0x5A:
  case 0x5B:
  case 0x5C:
  case 0x5D:
  case 0x5E:
  case 0x5F:
    OpEn_O;

  case 0x60:
  case 0x61:
  case 0x62:
    UnImplOpcode;

  case 0x63:
    if ((rex & REX_W) != 0) {
      OpEn_RM;
    };
    break;

  case 0x64:
  case 0x65:
  case 0x66:
  case 0x67:
    Op_Prefix;

  case 0x68:
    OpEn_I(16 | 32);

  case 0x69:
    OpEn_RMI(16 | 32);

  case 0x6a:
    OpEn_I(8);

  case 0x6b:
    OpEn_RMI(8);

  case 0x70:
  case 0x71:
  case 0x72:
  case 0x73:
  case 0x74:
  case 0x75:
  case 0x76:
  case 0x77:
  case 0x78:
  case 0x79:
  case 0x7A:
  case 0x7B:
  case 0x7C:
  case 0x7D:
  case 0x7E:
  case 0x7F:
    OpEn_D;

  case 0x80:
  case 0x81:
  case 0x82:
  case 0x83:
  case 0x84:
  case 0x85:
    UnImplOpcode;

  case 0x86:
  case 0x87:
    OpEn_RM;

  case 0x88:
  case 0x89:
  case 0x8a:
  case 0x8b:
    OpEn_RM;

  case 0x8c:
  case 0x8d:
  case 0x8e:
  case 0x8f:
  case 0x90:
  case 0x91:
  case 0x92:
  case 0x93:
  case 0x94:
  case 0x95:
  case 0x96:
  case 0x97:
  case 0x98:
  case 0x99:
  case 0x9a:
  case 0x9b:
  case 0x9c:
    UnImplOpcode;

  case 0x9d:
    OpEn_ZO;

  case 0x0f:
    DecodeExtendedOpcode
  }
}

void DecodeExtendedOpcode(uint8_t *instr) {
}