// Copyright (c) 2021-2024 ByteDance Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

// Created by Kelun Cai (caikelun@bytedance.com) on 2021-04-11.

#include "sh_t32.h"

#include <inttypes.h>
#include <stdint.h>

#include "sh_log.h"
#include "sh_util.h"

// https://developer.arm.com/documentation/ddi0406/latest
// https://developer.arm.com/documentation/ddi0597/latest

typedef enum {
  IGNORED = 0,
  B_T3,
  B_T4,
  BL_IMM_T1,
  BLX_IMM_T2,
  ADR_T2,
  ADR_T3,
  LDR_LIT_T2,
  LDR_LIT_PC_T2,
  LDRB_LIT_T1,
  LDRD_LIT_T1,
  LDRH_LIT_T1,
  LDRSB_LIT_T1,
  LDRSH_LIT_T1,
  PLD_LIT_T1,
  PLI_LIT_T3,
  TBB_T1,
  TBH_T1,
  VLDR_LIT_T1
} sh_t32_type_t;

static sh_t32_type_t sh_t32_get_type(uint32_t inst) {
  if (((inst & 0xF800D000) == 0xF0008000) && ((inst & 0x03800000u) != 0x03800000u))
    return B_T3;
  else if ((inst & 0xF800D000) == 0xF0009000)
    return B_T4;
  else if ((inst & 0xF800D000) == 0xF000D000)
    return BL_IMM_T1;
  else if ((inst & 0xF800D000) == 0xF000C000)
    return BLX_IMM_T2;
  else if ((inst & 0xFBFF8000) == 0xF2AF0000)
    return ADR_T2;
  else if ((inst & 0xFBFF8000) == 0xF20F0000)
    return ADR_T3;
  else if ((inst & 0xFF7F0000) == 0xF85F0000)
    return ((inst & 0x0000F000u) == 0x0000F000) ? LDR_LIT_PC_T2 : LDR_LIT_T2;
  else if (((inst & 0xFF7F0000) == 0xF81F0000) && ((inst & 0xF000u) != 0xF000u))
    return LDRB_LIT_T1;
  else if ((inst & 0xFF7F0000) == 0xE95F0000)
    return LDRD_LIT_T1;
  else if (((inst & 0xFF7F0000) == 0xF83F0000) && ((inst & 0xF000u) != 0xF000u))
    return LDRH_LIT_T1;
  else if (((inst & 0xFF7F0000) == 0xF91F0000) && ((inst & 0xF000u) != 0xF000u))
    return LDRSB_LIT_T1;
  else if (((inst & 0xFF7F0000) == 0xF93F0000) && ((inst & 0xF000u) != 0xF000u))
    return LDRSH_LIT_T1;
  else if ((inst & 0xFF7FF000) == 0xF81FF000)
    return PLD_LIT_T1;
  else if ((inst & 0xFF7FF000) == 0xF91FF000)
    return PLI_LIT_T3;
  else if ((inst & 0xFFF0FFF0) == 0xE8D0F000)
    return TBB_T1;
  else if ((inst & 0xFFF0FFF0) == 0xE8D0F010)
    return TBH_T1;
  else if ((inst & 0xFF3F0C00) == 0xED1F0800)
    return VLDR_LIT_T1;
  else
    return IGNORED;
}

size_t sh_t32_get_rewrite_inst_len(uint16_t high_inst, uint16_t low_inst) {
  static uint8_t map[] = {
      4,   // IGNORED
      12,  // B_T3
      8,   // B_T4
      12,  // BL_IMM_T1
      12,  // BLX_IMM_T2
      12,  // ADR_T2
      12,  // ADR_T3
      16,  // LDR_LIT_T2
      24,  // LDR_LIT_PC_T2
      16,  // LDRB_LIT_T1
      16,  // LDRD_LIT_T1
      16,  // LDRH_LIT_T1
      16,  // LDRSB_LIT_T1
      16,  // LDRSH_LIT_T1
      20,  // PLD_LIT_T1
      20,  // PLI_LIT_T3
      32,  // TBB_T1
      32,  // TBH_T1
      24   // VLDR_LIT_T1
  };

  uint32_t inst = (uint32_t)(high_inst << 16u) | low_inst;
  return (size_t)(map[sh_t32_get_type(inst)]);
}

static size_t sh_t32_rewrite_b(uint16_t *buf, uint16_t high_inst, uint16_t low_inst, uintptr_t pc,
                               sh_t32_type_t type, sh_txx_rewrite_info_t *rinfo) {
  uint32_t j1 = SH_UTIL_GET_BIT_16(low_inst, 13);
  uint32_t j2 = SH_UTIL_GET_BIT_16(low_inst, 11);
  uint32_t s = SH_UTIL_GET_BIT_16(high_inst, 10);
  uint32_t i1 = !(j1 ^ s);
  uint32_t i2 = !(j2 ^ s);

  uint32_t addr;
  if (type == B_T3) {
    uint32_t x =
        (s << 20u) | (j2 << 19u) | (j1 << 18u) | ((high_inst & 0x3Fu) << 12u) | ((low_inst & 0x7FFu) << 1u);
    uint32_t imm32 = SH_UTIL_SIGN_EXTEND_32(x, 21u);
    addr = SH_UTIL_SET_BIT0(pc + imm32);  // thumb -> thumb
  } else if (type == B_T4) {
    uint32_t x =
        (s << 24u) | (i1 << 23u) | (i2 << 22u) | ((high_inst & 0x3FFu) << 12u) | ((low_inst & 0x7FFu) << 1u);
    uint32_t imm32 = SH_UTIL_SIGN_EXTEND_32(x, 25u);
    addr = SH_UTIL_SET_BIT0(pc + imm32);  // thumb -> thumb
  } else if (type == BL_IMM_T1) {
    uint32_t x =
        (s << 24u) | (i1 << 23u) | (i2 << 22u) | ((high_inst & 0x3FFu) << 12u) | ((low_inst & 0x7FFu) << 1u);
    uint32_t imm32 = SH_UTIL_SIGN_EXTEND_32(x, 25u);
    addr = SH_UTIL_SET_BIT0(pc + imm32);  // thumb -> thumb
  } else                                  // type == BLX_IMM_T2
  {
    uint32_t x =
        (s << 24u) | (i1 << 23u) | (i2 << 22u) | ((high_inst & 0x3FFu) << 12u) | ((low_inst & 0x7FEu) << 1u);
    uint32_t imm32 = SH_UTIL_SIGN_EXTEND_32(x, 25u);
    // In BL and BLX instructions, only when the target instruction set is "arm",
    // you need to do 4-byte alignment for PC.
    addr = SH_UTIL_ALIGN_4(pc) + imm32;  // thumb -> arm, align4
  }
  addr = sh_txx_fix_addr(addr, rinfo);

  size_t idx = 0;
  if (type == B_T3) {
    uint32_t cond = SH_UTIL_GET_BITS_16(high_inst, 9, 6);
    buf[idx++] = 0xD000u | (uint16_t)(cond << 8u);  // B<c> #0
    buf[idx++] = 0xE003;                            // B #6
  } else if (type == BL_IMM_T1 || type == BLX_IMM_T2) {
    buf[idx++] = 0xF20F;  // ADD LR, PC, #9
    buf[idx++] = 0x0E09;  // ...
  }
  buf[idx++] = 0xF8DF;  // LDR.W PC, [PC]
  buf[idx++] = 0xF000;  // ...
  buf[idx++] = addr & 0xFFFFu;
  buf[idx++] = addr >> 16u;
  return idx * 2;  // 8 or 12
}

static size_t sh_t32_rewrite_adr(uint16_t *buf, uint16_t high_inst, uint16_t low_inst, uintptr_t pc,
                                 sh_t32_type_t type, sh_txx_rewrite_info_t *rinfo) {
  uint32_t rt = SH_UTIL_GET_BITS_16(low_inst, 11, 8);  // r0 - r14
  uint32_t i = SH_UTIL_GET_BIT_16(high_inst, 10);
  uint32_t imm3 = SH_UTIL_GET_BITS_16(low_inst, 14, 12);
  uint32_t imm8 = SH_UTIL_GET_BITS_16(low_inst, 7, 0);
  uint32_t imm32 = (i << 11u) | (imm3 << 8u) | imm8;
  uint32_t addr = (type == ADR_T2 ? (SH_UTIL_ALIGN_4(pc) - imm32) : (SH_UTIL_ALIGN_4(pc) + imm32));
  if (sh_txx_is_addr_need_fix(addr, rinfo)) return 0;  // rewrite failed

  buf[0] = 0xF8DF;                      // LDR.W Rt, [PC, #4]
  buf[1] = (uint16_t)(rt << 12u) + 4u;  // ...
  buf[2] = 0xE002;                      // B #4
  buf[3] = 0xBF00;                      // NOP
  buf[4] = addr & 0xFFFFu;
  buf[5] = addr >> 16u;
  return 12;
}

static size_t sh_t32_rewrite_ldr(uint16_t *buf, uint16_t high_inst, uint16_t low_inst, uintptr_t pc,
                                 sh_t32_type_t type, sh_txx_rewrite_info_t *rinfo) {
  uint32_t u = SH_UTIL_GET_BIT_16(high_inst, 7);
  uint32_t rt = SH_UTIL_GET_BITS_16(low_inst, 15, 12);  // r0 - r15
  uint32_t rt2 = 0;                                     // r0 - r15
  uint32_t addr;

  if (type == LDRD_LIT_T1) {
    rt2 = SH_UTIL_GET_BITS_16(low_inst, 11, 8);
    uint32_t imm8 = SH_UTIL_GET_BITS_16(low_inst, 7, 0);
    addr = (u ? SH_UTIL_ALIGN_4(pc) + (imm8 << 2u) : SH_UTIL_ALIGN_4(pc) - (imm8 << 2u));
  } else {
    uint32_t imm12 = (uint32_t)SH_UTIL_GET_BITS_16(low_inst, 11, 0);
    addr = (u ? SH_UTIL_ALIGN_4(pc) + imm12 : SH_UTIL_ALIGN_4(pc) - imm12);
  }
  if (sh_txx_is_addr_need_fix(addr, rinfo)) return 0;  // rewrite failed

  if (type == LDR_LIT_PC_T2 && rt == 0xF)  // Rt == PC
  {
    buf[0] = 0xB403;          // PUSH {R0, R1}
    buf[1] = 0xBF00;          // NOP
    buf[2] = 0xF8DF;          // LDR.W R0, [PC, #4]
    buf[3] = 0x0004;          // ...
    buf[4] = 0xE002;          // B #4
    buf[5] = 0xBF00;          // NOP
    buf[6] = addr & 0xFFFFu;  //
    buf[7] = addr >> 16u;     //
    buf[8] = 0xF8D0;          // LDR.W R0, [R0]
    buf[9] = 0x0000;          // ...
    buf[10] = 0x9001;         // STR R0, [SP, #4]
    buf[11] = 0xBD01;         // POP {R0, PC}
    return 24;
  } else {
    buf[0] = 0xF8DF;                      // LDR.W Rt, [PC, #4]
    buf[1] = (uint16_t)(rt << 12u) | 4u;  // ...
    buf[2] = 0xE002;                      // B #4
    buf[3] = 0xBF00;                      // NOP
    buf[4] = addr & 0xFFFFu;
    buf[5] = addr >> 16u;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
    switch (type) {
      case LDR_LIT_T2:
        buf[6] = (uint16_t)(0xF8D0 + rt);  // LDR.W Rt, [Rt]
        buf[7] = (uint16_t)(rt << 12u);    // ...
        break;
      case LDRB_LIT_T1:
        buf[6] = (uint16_t)(0xF890 + rt);  // LDRB.W Rt, [Rt]
        buf[7] = (uint16_t)(rt << 12u);    // ...
        break;
      case LDRD_LIT_T1:
        buf[6] = (uint16_t)(0xE9D0 + rt);                        // LDRD Rt, Rt2, [Rt]
        buf[7] = (uint16_t)(rt << 12u) + (uint16_t)(rt2 << 8u);  // ...
        break;
      case LDRH_LIT_T1:
        buf[6] = (uint16_t)(0xF8B0 + rt);  // LDRH.W Rt, [Rt]
        buf[7] = (uint16_t)(rt << 12u);    // ...
        break;
      case LDRSB_LIT_T1:
        buf[6] = (uint16_t)(0xF990 + rt);  // LDRSB.W Rt, [Rt]
        buf[7] = (uint16_t)(rt << 12u);    // ...
        break;
      case LDRSH_LIT_T1:
        buf[6] = (uint16_t)(0xF9B0 + rt);  // LDRSH.W Rt, [Rt]
        buf[7] = (uint16_t)(rt << 12u);    // ...
        break;
    }
#pragma clang diagnostic pop
    return 16;
  }
}

static size_t sh_t32_rewrite_pl(uint16_t *buf, uint16_t high_inst, uint16_t low_inst, uintptr_t pc,
                                sh_t32_type_t type, sh_txx_rewrite_info_t *rinfo) {
  uint32_t u = SH_UTIL_GET_BIT_16(high_inst, 7);
  uint32_t imm12 = SH_UTIL_GET_BITS_16(low_inst, 11, 0);
  uint32_t addr = (u ? SH_UTIL_ALIGN_4(pc) + imm12 : SH_UTIL_ALIGN_4(pc) - imm12);
  addr = sh_txx_fix_addr(addr, rinfo);

  buf[0] = 0xB401;  // PUSH {R0}
  buf[1] = 0xBF00;  // NOP
  buf[2] = 0xF8DF;  // LDR.W R0, [PC, #8]
  buf[3] = 0x0008;  // ...
  if (type == PLD_LIT_T1) {
    buf[4] = 0xF890;  // PLD [R0]
    buf[5] = 0xF000;  // ...
  } else {
    buf[4] = 0xF990;  // PLI [R0]
    buf[5] = 0xF000;  // ...
  }
  buf[6] = 0xBC01;  // POP {R0}
  buf[7] = 0xE001;  // B #2
  buf[8] = addr & 0xFFFFu;
  buf[9] = addr >> 16u;
  return 20;
}

static size_t sh_t32_rewrite_tb(uint16_t *buf, uint16_t high_inst, uint16_t low_inst, uintptr_t pc,
                                sh_t32_type_t type, sh_txx_rewrite_info_t *rinfo) {
  // If TBB/TBH is not the last instruction that needs to be rewritten,
  // the rewriting can NOT be completed.
  uintptr_t target_addr = SH_UTIL_CLEAR_BIT0(pc - 4);
  if (target_addr + 4 != rinfo->end_addr) return 0;  // rewrite failed

  uint32_t rn = SH_UTIL_GET_BITS_16(high_inst, 3, 0);
  uint32_t rm = SH_UTIL_GET_BITS_16(low_inst, 3, 0);
  uint32_t rx, ry;  // r0 - r7
  for (rx = 7;; --rx)
    if (rx != rn && rx != rm) break;
  for (ry = 7;; --ry)
    if (ry != rn && ry != rm && ry != rx) break;

  buf[0] = (uint16_t)(0xB500u | (1u << rx) | (1u << ry));  // PUSH {Rx, Ry, LR}
  buf[1] = 0xBF00;                                         // NOP
  buf[2] = 0xF8DF;                                         // LDR.W Rx, [PC, #20]
  buf[3] = (uint16_t)(rx << 12u) | 20u;                    // ...
  if (type == TBB_T1) {
    buf[4] = (uint16_t)(0xEB00u | (rn == 0xF ? rx : rn));  // ADD.W Ry, Rx|Rn, Rm
    buf[5] = (uint16_t)(0x0000u | (ry << 8u) | rm);        // ...
    buf[6] = (uint16_t)(0x7800u | (ry << 3u) | ry);        // LDRB Ry, [Ry]
    buf[7] = 0xBF00;                                       // NOP
  } else {
    buf[4] = (uint16_t)(0xEB00u | (rn == 0xF ? rx : rn));  // ADD.W Ry, Rx|Rn, Rm, LSL #1
    buf[5] = (uint16_t)(0x0040u | (ry << 8u) | rm);        // ...
    buf[6] = (uint16_t)(0x8800u | (ry << 3u) | ry);        // LDRH Ry, [Ry]
    buf[7] = 0xBF00;                                       // NOP
  }
  buf[8] = (uint16_t)(0xEB00u | rx);                        // ADD Rx, Rx, Ry, LSL #1
  buf[9] = (uint16_t)(0x0040u | (rx << 8u) | ry);           // ...
  buf[10] = (uint16_t)(0x3001u | (rx << 8u));               // ADD Rx, #1
  buf[11] = (uint16_t)(0x9002u | (rx << 8u));               // STR Rx, [SP, #8]
  buf[12] = (uint16_t)(0xBD00u | (1u << rx) | (1u << ry));  // POP {Rx, Ry, PC}
  buf[13] = 0xBF00;                                         // NOP
  buf[14] = pc & 0xFFFFu;
  buf[15] = pc >> 16u;
  return 32;
}

static size_t sh_t32_rewrite_vldr(uint16_t *buf, uint16_t high_inst, uint16_t low_inst, uintptr_t pc,
                                  sh_txx_rewrite_info_t *rinfo) {
  uint32_t u = SH_UTIL_GET_BIT_16(high_inst, 7);
  uint32_t D = SH_UTIL_GET_BIT_16(high_inst, 6);
  uint32_t vd = SH_UTIL_GET_BITS_16(low_inst, 15, 12);
  uint32_t size = SH_UTIL_GET_BITS_16(low_inst, 9, 8);
  uint32_t imm8 = SH_UTIL_GET_BITS_16(low_inst, 7, 0);
  uint32_t esize = (8u << size);
  uint32_t imm32 = (esize == 16 ? imm8 << 1u : imm8 << 2u);
  uint32_t addr = (u ? SH_UTIL_ALIGN_4(pc) + imm32 : SH_UTIL_ALIGN_4(pc) - imm32);
  if (sh_txx_is_addr_need_fix(addr, rinfo)) return 0;  // rewrite failed

  buf[0] = 0xB401;                                       // PUSH {R0}
  buf[1] = 0xBF00;                                       // NOP
  buf[2] = 0xF8DF;                                       // LDR.W R0, [PC, #4]
  buf[3] = 0x0004;                                       // ...
  buf[4] = 0xE002;                                       // B #4
  buf[5] = 0xBF00;                                       // NOP
  buf[6] = addr & 0xFFFFu;                               //
  buf[7] = addr >> 16u;                                  //
  buf[8] = (uint16_t)(0xED90u | D << 6u);                // VLDR Sd|Dd, [R0]
  buf[9] = (uint16_t)(0x800u | vd << 12u | size << 8u);  // ...
  buf[10] = 0xBC01;                                      // POP {R0}
  buf[11] = 0xBF00;                                      // NOP
  return 24;
}

size_t sh_t32_rewrite(uint16_t *buf, uint16_t high_inst, uint16_t low_inst, uintptr_t pc,
                      sh_txx_rewrite_info_t *rinfo) {
  uint32_t inst = (uint32_t)(high_inst << 16u) | low_inst;
  sh_t32_type_t type = sh_t32_get_type(inst);
  SH_LOG_INFO("t32 rewrite: type %d, high inst %" PRIx16 ", low inst %" PRIx16, type, high_inst, low_inst);

  if (type == B_T3 || type == B_T4 || type == BL_IMM_T1 || type == BLX_IMM_T2)
    return sh_t32_rewrite_b(buf, high_inst, low_inst, pc, type, rinfo);
  else if (type == ADR_T2 || type == ADR_T3)
    return sh_t32_rewrite_adr(buf, high_inst, low_inst, pc, type, rinfo);
  else if (type == LDR_LIT_T2 || type == LDR_LIT_PC_T2 || type == LDRB_LIT_T1 || type == LDRD_LIT_T1 ||
           type == LDRH_LIT_T1 || type == LDRSB_LIT_T1 || type == LDRSH_LIT_T1)
    return sh_t32_rewrite_ldr(buf, high_inst, low_inst, pc, type, rinfo);
  else if (type == PLD_LIT_T1 || type == PLI_LIT_T3)
    return sh_t32_rewrite_pl(buf, high_inst, low_inst, pc, type, rinfo);
  else if (type == TBB_T1 || type == TBH_T1)
    return sh_t32_rewrite_tb(buf, high_inst, low_inst, pc, type, rinfo);
  else if (type == VLDR_LIT_T1)
    return sh_t32_rewrite_vldr(buf, high_inst, low_inst, pc, rinfo);
  else {
    // IGNORED
    buf[0] = high_inst;
    buf[1] = low_inst;
    return 4;
  }
}

size_t sh_t32_absolute_jump(uint16_t *buf, bool is_align4, uintptr_t addr) {
  size_t i = 0;
  if (!is_align4) buf[i++] = 0xBF00;  // NOP
  buf[i++] = 0xF8DF;                  // LDR.W PC, [PC]
  buf[i++] = 0xF000;                  // ...
  buf[i++] = addr & 0xFFFFu;
  buf[i++] = addr >> 16u;
  return i * 2;
}

size_t sh_t32_relative_jump(uint16_t *buf, uintptr_t addr, uintptr_t pc) {
  uint32_t imm32 = addr - pc;
  uint32_t s = SH_UTIL_GET_BIT_32(imm32, 24);
  uint32_t i1 = SH_UTIL_GET_BIT_32(imm32, 23);
  uint32_t i2 = SH_UTIL_GET_BIT_32(imm32, 22);
  uint32_t imm10 = SH_UTIL_GET_BITS_32(imm32, 21, 12);
  uint32_t imm11 = SH_UTIL_GET_BITS_32(imm32, 11, 1);
  uint32_t j1 = (!i1) ^ s;
  uint32_t j2 = (!i2) ^ s;

  buf[0] = (uint16_t)(0xF000u | (s << 10u) | imm10);
  buf[1] = (uint16_t)(0x9000u | (j1 << 13u) | (j2 << 11u) | imm11);
  return 4;
}
