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

#include "sh_a32.h"

#include <inttypes.h>
#include <sh_util.h>
#include <stdint.h>

#include "sh_log.h"

// https://developer.arm.com/documentation/ddi0406/latest
// https://developer.arm.com/documentation/ddi0597/latest

typedef enum {
  IGNORED = 0,
  B_A1,
  BX_A1,
  BL_IMM_A1,
  BLX_IMM_A2,
  ADD_REG_A1,
  ADD_REG_PC_A1,
  SUB_REG_A1,
  SUB_REG_PC_A1,
  ADR_A1,
  ADR_A2,
  MOV_REG_A1,
  MOV_REG_PC_A1,
  LDR_LIT_A1,
  LDR_LIT_PC_A1,
  LDRB_LIT_A1,
  LDRD_LIT_A1,
  LDRH_LIT_A1,
  LDRSB_LIT_A1,
  LDRSH_LIT_A1,
  LDR_REG_A1,
  LDR_REG_PC_A1,
  LDRB_REG_A1,
  LDRD_REG_A1,
  LDRH_REG_A1,
  LDRSB_REG_A1,
  LDRSH_REG_A1
} sh_a32_type_t;

static sh_a32_type_t sh_a32_get_type(uint32_t inst) {
  if (((inst & 0x0F000000u) == 0x0A000000) && ((inst & 0xF0000000) != 0xF0000000))
    return B_A1;
  else if (((inst & 0x0FFFFFFFu) == 0x012FFF1F) && ((inst & 0xF0000000) != 0xF0000000))
    return BX_A1;
  else if (((inst & 0x0F000000u) == 0x0B000000) && ((inst & 0xF0000000) != 0xF0000000))
    return BL_IMM_A1;
  else if ((inst & 0xFE000000) == 0xFA000000)
    return BLX_IMM_A2;
  else if (((inst & 0x0FE00010u) == 0x00800000) && ((inst & 0xF0000000) != 0xF0000000) &&
           ((inst & 0x0010F000u) != 0x0010F000) && ((inst & 0x000F0000u) != 0x000D0000) &&
           (((inst & 0x000F0000u) == 0x000F0000) || ((inst & 0x0000000Fu) == 0x0000000F)))
    return ((inst & 0x0000F000u) == 0x0000F000) ? ADD_REG_PC_A1 : ADD_REG_A1;
  else if (((inst & 0x0FE00010u) == 0x00400000) && ((inst & 0xF0000000) != 0xF0000000) &&
           ((inst & 0x0010F000u) != 0x0010F000) && ((inst & 0x000F0000u) != 0x000D0000) &&
           (((inst & 0x000F0000u) == 0x000F0000) || ((inst & 0x0000000Fu) == 0x0000000F)))
    return ((inst & 0x0000F000u) == 0x0000F000) ? SUB_REG_PC_A1 : SUB_REG_A1;
  else if (((inst & 0x0FFF0000u) == 0x028F0000) && ((inst & 0xF0000000) != 0xF0000000))
    return ADR_A1;
  else if (((inst & 0x0FFF0000u) == 0x024F0000) && ((inst & 0xF0000000) != 0xF0000000))
    return ADR_A2;
  else if (((inst & 0x0FEF001Fu) == 0x01A0000F) && ((inst & 0xF0000000) != 0xF0000000) &&
           ((inst & 0x0010F000u) != 0x0010F000) &&
           (!(((inst & 0x0000F000u) == 0x0000F000) && ((inst & 0x00000FF0u) != 0x00000000))))
    return ((inst & 0x0000F000u) == 0x0000F000) ? MOV_REG_PC_A1 : MOV_REG_A1;
  else if (((inst & 0x0F7F0000u) == 0x051F0000) && ((inst & 0xF0000000) != 0xF0000000))
    return ((inst & 0x0000F000u) == 0x0000F000) ? LDR_LIT_PC_A1 : LDR_LIT_A1;
  else if (((inst & 0x0F7F0000u) == 0x055F0000) && ((inst & 0xF0000000) != 0xF0000000))
    return LDRB_LIT_A1;
  else if (((inst & 0x0F7F00F0u) == 0x014F00D0) && ((inst & 0xF0000000) != 0xF0000000))
    return LDRD_LIT_A1;
  else if (((inst & 0x0F7F00F0u) == 0x015F00B0) && ((inst & 0xF0000000) != 0xF0000000))
    return LDRH_LIT_A1;
  else if (((inst & 0x0F7F00F0u) == 0x015F00D0) && ((inst & 0xF0000000) != 0xF0000000))
    return LDRSB_LIT_A1;
  else if (((inst & 0x0F7F00F0u) == 0x015F00F0) && ((inst & 0xF0000000) != 0xF0000000))
    return LDRSH_LIT_A1;
  else if (((inst & 0x0E5F0010u) == 0x061F0000) && ((inst & 0xF0000000) != 0xF0000000) &&
           ((inst & 0x01200000u) != 0x00200000))
    return ((inst & 0x0000F000u) == 0x0000F000) ? LDR_REG_PC_A1 : LDR_REG_A1;
  else if (((inst & 0x0E5F0010u) == 0x065F0000) && ((inst & 0xF0000000) != 0xF0000000) &&
           ((inst & 0x01200000u) != 0x00200000))
    return LDRB_REG_A1;
  else if (((inst & 0x0E5F0FF0u) == 0x000F00D0) && ((inst & 0xF0000000) != 0xF0000000) &&
           ((inst & 0x01200000u) != 0x00200000))
    return LDRD_REG_A1;
  else if (((inst & 0x0E5F0FF0u) == 0x001F00B0) && ((inst & 0xF0000000) != 0xF0000000) &&
           ((inst & 0x01200000u) != 0x00200000))
    return LDRH_REG_A1;
  else if (((inst & 0x0E5F0FF0u) == 0x001F00D0) && ((inst & 0xF0000000) != 0xF0000000) &&
           ((inst & 0x01200000u) != 0x00200000))
    return LDRSB_REG_A1;
  else if (((inst & 0x0E5F0FF0u) == 0x001F00F0) && ((inst & 0xF0000000) != 0xF0000000) &&
           ((inst & 0x01200000u) != 0x00200000))
    return LDRSH_REG_A1;
  else
    return IGNORED;
}

size_t sh_a32_get_rewrite_inst_len(uint32_t inst) {
  static uint8_t map[] = {
      4,   // IGNORED
      12,  // B_A1
      12,  // BX_A1
      16,  // BL_IMM_A1
      16,  // BLX_IMM_A2
      32,  // ADD_REG_A1
      32,  // ADD_REG_PC_A1
      32,  // SUB_REG_A1
      32,  // SUB_REG_PC_A1
      12,  // ADR_A1
      12,  // ADR_A2
      32,  // MOV_REG_A1
      12,  // MOV_REG_PC_A1
      24,  // LDR_LIT_A1
      36,  // LDR_LIT_PC_A1
      24,  // LDRB_LIT_A1
      24,  // LDRD_LIT_A1
      24,  // LDRH_LIT_A1
      24,  // LDRSB_LIT_A1
      24,  // LDRSH_LIT_A1
      32,  // LDR_REG_A1
      36,  // LDR_REG_PC_A1
      32,  // LDRB_REG_A1
      32,  // LDRD_REG_A1
      32,  // LDRH_REG_A1
      32,  // LDRSB_REG_A1
      32   // LDRSH_REG_A1
  };

  return (size_t)(map[sh_a32_get_type(inst)]);
}

static bool sh_a32_is_addr_need_fix(uintptr_t addr, sh_a32_rewrite_info_t *rinfo) {
  return (rinfo->overwrite_start_addr <= addr && addr < rinfo->overwrite_end_addr);
}

static uintptr_t sh_a32_fix_addr(uintptr_t addr, sh_a32_rewrite_info_t *rinfo) {
  if (rinfo->overwrite_start_addr <= addr && addr < rinfo->overwrite_end_addr) {
    uintptr_t cursor_addr = rinfo->overwrite_start_addr;
    size_t offset = 0;
    for (size_t i = 0; i < rinfo->rewrite_inst_lens_cnt; i++) {
      if (cursor_addr >= addr) break;
      cursor_addr += 4;
      offset += rinfo->rewrite_inst_lens[i];
    }
    uintptr_t fixed_addr = (uintptr_t)rinfo->rewrite_buf + offset;
    SH_LOG_INFO("a32 rewrite: fix addr %" PRIxPTR " -> %" PRIxPTR, addr, fixed_addr);
    return fixed_addr;
  }

  return addr;
}

static size_t sh_a32_rewrite_b(uint32_t *buf, uint32_t inst, uintptr_t pc, sh_a32_type_t type,
                               sh_a32_rewrite_info_t *rinfo) {
  uint32_t cond;
  if (type == B_A1 || type == BL_IMM_A1 || type == BX_A1)
    cond = SH_UTIL_GET_BITS_32(inst, 31, 28);
  else
    // type == BLX_IMM_A2
    cond = 0xE;  // 1110 None (AL)

  uint32_t addr;
  if (type == B_A1 || type == BL_IMM_A1) {
    uint32_t imm24 = SH_UTIL_GET_BITS_32(inst, 23, 0);
    uint32_t imm32 = SH_UTIL_SIGN_EXTEND_32(imm24 << 2u, 26u);
    addr = pc + imm32;  // arm -> arm
  } else if (type == BLX_IMM_A2) {
    uint32_t h = SH_UTIL_GET_BIT_32(inst, 24);
    uint32_t imm24 = SH_UTIL_GET_BITS_32(inst, 23, 0);
    uint32_t imm32 = SH_UTIL_SIGN_EXTEND_32((imm24 << 2u) | (h << 1u), 26u);
    addr = SH_UTIL_SET_BIT0(pc + imm32);  // arm -> thumb
  } else {
    // type == BX_A1
    // BX PC
    // PC must be even, and the "arm" instruction must be at a 4-byte aligned address,
    // so the instruction set must keep "arm" unchanged.
    addr = pc;  // arm -> arm
  }
  addr = sh_a32_fix_addr(addr, rinfo);

  size_t idx = 0;
  if (type == BL_IMM_A1 || type == BLX_IMM_A2) {
    buf[idx++] = 0x028FE008u | (cond << 28u);  // ADD<c> LR, PC, #8
  }
  buf[idx++] = 0x059FF000u | (cond << 28u);  // LDR<c> PC, [PC, #0]
  buf[idx++] = 0xEA000000;                   // B #0
  buf[idx++] = addr;
  return idx * 4;  // 12 or 16
}

static size_t sh_a32_rewrite_add_or_sub(uint32_t *buf, uint32_t inst, uintptr_t pc) {
  // ADD{S}<c> <Rd>, <Rn>, PC{, <shift>}  or  ADD{S}<c> <Rd>, PC, <Rm>{, <shift>}
  // SUB{S}<c> <Rd>, <Rn>, PC{, <shift>}  or  SUB{S}<c> <Rd>, PC, <Rm>{, <shift>}
  uint32_t cond = SH_UTIL_GET_BITS_32(inst, 31, 28);
  uint32_t rn = SH_UTIL_GET_BITS_32(inst, 19, 16);
  uint32_t rm = SH_UTIL_GET_BITS_32(inst, 3, 0);
  uint32_t rd = SH_UTIL_GET_BITS_32(inst, 15, 12);

  uint32_t rx;  // r0 - r3
  for (rx = 3;; --rx)
    if (rx != rn && rx != rm && rx != rd) break;

  if (rd == 0xF)  // Rd == PC
  {
    uint32_t ry;  // r0 - r4
    for (ry = 4;; --ry)
      if (ry != rn && ry != rm && ry != rd && ry != rx) break;

    buf[0] = 0x0A000000u | (cond << 28u);           // B<c> #0
    buf[1] = 0xEA000005;                            // B #20
    buf[2] = 0xE92D8000 | (1u << rx) | (1u << ry);  // PUSH {Rx, Ry, PC}
    buf[3] = 0xE59F0008 | (rx << 12u);              // LDR Rx, [PC, #8]
    if (rn == 0xF)
      // Rn == PC
      buf[4] =
          (inst & 0x0FF00FFFu) | 0xE0000000 | (ry << 12u) | (rx << 16u);  // ADD/SUB Ry, Rx, Rm{, <shift>}
    else
      // Rm == PC
      buf[4] = (inst & 0x0FFF0FF0u) | 0xE0000000 | (ry << 12u) | rx;  // ADD/SUB Ry, Rn, Rx{, <shift>}
    buf[5] = 0xE58D0008 | (ry << 12u);                                // STR Ry, [SP, #8]
    buf[6] = 0xE8BD8000 | (1u << rx) | (1u << ry);                    // POP {Rx, Ry, PC}
    buf[7] = pc;
    return 32;
  } else {
    buf[0] = 0x0A000000u | (cond << 28u);  // B<c> #0
    buf[1] = 0xEA000005;                   // B #20
    buf[2] = 0xE52D0004 | (rx << 12u);     // PUSH {Rx}
    buf[3] = 0xE59F0008 | (rx << 12u);     // LDR Rx, [PC, #8]
    if (rn == 0xF)
      // Rn == PC
      buf[4] = (inst & 0x0FF0FFFFu) | 0xE0000000 | (rx << 16u);  // ADD/SUB{S} Rd, Rx, Rm{, <shift>}
    else
      // Rm == PC
      buf[4] = (inst & 0x0FFFFFF0u) | 0xE0000000 | rx;  // ADD/SUB{S} Rd, Rn, Rx{, <shift>}
    buf[5] = 0xE49D0004 | (rx << 12u);                  // POP {Rx}
    buf[6] = 0xEA000000;                                // B #0
    buf[7] = pc;
    return 32;
  }
}

static size_t sh_a32_rewrite_adr(uint32_t *buf, uint32_t inst, uintptr_t pc, sh_a32_type_t type,
                                 sh_a32_rewrite_info_t *rinfo) {
  uint32_t cond = SH_UTIL_GET_BITS_32(inst, 31, 28);
  uint32_t rd = SH_UTIL_GET_BITS_32(inst, 15, 12);  // r0 - r15
  uint32_t imm12 = SH_UTIL_GET_BITS_32(inst, 11, 0);
  uint32_t imm32 = sh_util_arm_expand_imm(imm12);
  uint32_t addr = (type == ADR_A1 ? (SH_UTIL_ALIGN_4(pc) + imm32) : (SH_UTIL_ALIGN_4(pc) - imm32));
  if (sh_a32_is_addr_need_fix(addr, rinfo)) return 0;  // rewrite failed

  buf[0] = 0x059F0000u | (cond << 28u) | (rd << 12u);  // LDR<c> Rd, [PC, #0]
  buf[1] = 0xEA000000;                                 // B #0
  buf[2] = addr;
  return 12;
}

static size_t sh_a32_rewrite_mov(uint32_t *buf, uint32_t inst, uintptr_t pc) {
  // MOV{S}<c> <Rd>, PC
  uint32_t cond = SH_UTIL_GET_BITS_32(inst, 31, 28);
  uint32_t rd = SH_UTIL_GET_BITS_32(inst, 15, 12);
  uint32_t rx = (rd == 0) ? 1 : 0;

  if (rd == 0xF)  // Rd == PC (MOV PC, PC)
  {
    buf[0] = 0x059FF000u | (cond << 28u);  // LDR<c> PC, [PC, #0]
    buf[1] = 0xEA000000;                   // B #0
    buf[2] = pc;
    return 12;
  } else {
    buf[0] = 0x0A000000u | (cond << 28u);             // B<c> #0
    buf[1] = 0xEA000005;                              // B #20
    buf[2] = 0xE52D0004 | (rx << 12u);                // PUSH {Rx}
    buf[3] = 0xE59F0008 | (rx << 12u);                // LDR Rx, [PC, #8]
    buf[4] = (inst & 0x0FFFFFF0u) | 0xE0000000 | rx;  // MOV{S} Rd, Rx{, <shift> #<amount>/RRX}
    buf[5] = 0xE49D0004 | (rx << 12u);                // POP {Rx}
    buf[6] = 0xEA000000;                              // B #0
    buf[7] = pc;
    return 32;
  }
}

static size_t sh_a32_rewrite_ldr_lit(uint32_t *buf, uint32_t inst, uintptr_t pc, sh_a32_type_t type,
                                     sh_a32_rewrite_info_t *rinfo) {
  uint32_t cond = SH_UTIL_GET_BITS_32(inst, 31, 28);
  uint32_t u = SH_UTIL_GET_BIT_32(inst, 23);
  uint32_t rt = SH_UTIL_GET_BITS_16(inst, 15, 12);

  uint32_t imm32;
  if (type == LDR_LIT_A1 || type == LDR_LIT_PC_A1 || type == LDRB_LIT_A1)
    imm32 = SH_UTIL_GET_BITS_32(inst, 11, 0);
  else
    imm32 = (SH_UTIL_GET_BITS_32(inst, 11, 8) << 4u) + SH_UTIL_GET_BITS_32(inst, 3, 0);
  uint32_t addr = (u ? (SH_UTIL_ALIGN_4(pc) + imm32) : (SH_UTIL_ALIGN_4(pc) - imm32));
  if (sh_a32_is_addr_need_fix(addr, rinfo)) return 0;  // rewrite failed

  if (type == LDR_LIT_PC_A1 && rt == 0xF) {
    // Rt == PC
    buf[0] = 0x0A000000u | (cond << 28u);  // B<c> #0
    buf[1] = 0xEA000006;                   // B #24
    buf[2] = 0xE92D0003;                   // PUSH {R0, R1}
    buf[3] = 0xE59F0000;                   // LDR R0, [PC, #0]
    buf[4] = 0xEA000000;                   // B #0
    buf[5] = addr;                         //
    buf[6] = 0xE5900000;                   // LDR R0, [R0]
    buf[7] = 0xE58D0004;                   // STR R0, [SP, #4]
    buf[8] = 0xE8BD8001;                   // POP {R0, PC}
    return 36;
  } else {
    buf[0] = 0x0A000000u | (cond << 28u);  // B<c> #0
    buf[1] = 0xEA000003;                   // B #12
    buf[2] = 0xE59F0000 | (rt << 12u);     // LDR Rt, [PC, #0]
    buf[3] = 0xEA000000;                   // B #0
    buf[4] = addr;                         //
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wswitch"
    switch (type) {
      case LDR_LIT_A1:
        buf[5] = 0xE5900000 | (rt << 16u) | (rt << 12u);  // LDR Rt, [Rt]
        break;
      case LDRB_LIT_A1:
        buf[5] = 0xE5D00000 | (rt << 16u) | (rt << 12u);  // LDRB Rt, [Rt]
        break;
      case LDRD_LIT_A1:
        buf[5] = 0xE1C000D0 | (rt << 16u) | (rt << 12u);  // LDRD Rt, [Rt]
        break;
      case LDRH_LIT_A1:
        buf[5] = 0xE1D000B0 | (rt << 16u) | (rt << 12u);  // LDRH Rt, [Rt]
        break;
      case LDRSB_LIT_A1:
        buf[5] = 0xE1D000D0 | (rt << 16u) | (rt << 12u);  // LDRSB Rt, [Rt]
        break;
      case LDRSH_LIT_A1:
        buf[5] = 0xE1D000F0 | (rt << 16u) | (rt << 12u);  // LDRSH Rt, [Rt]
        break;
    }
#pragma clang diagnostic pop
    return 24;
  }
}

static size_t sh_a32_rewrite_ldr_reg(uint32_t *buf, uint32_t inst, uintptr_t pc, sh_a32_type_t type) {
  // LDR<c> <Rt>, [PC,+/-<Rm>{, <shift>}]{!}
  // ......
  uint32_t cond = SH_UTIL_GET_BITS_32(inst, 31, 28);
  uint32_t rt = SH_UTIL_GET_BITS_16(inst, 15, 12);
  uint32_t rt2 = rt + 1;
  uint32_t rm = SH_UTIL_GET_BITS_16(inst, 3, 0);
  uint32_t rx;  // r0 - r3
  for (rx = 3;; --rx)
    if (rx != rt && rx != rt2 && rx != rm) break;

  if (type == LDR_REG_PC_A1 && rt == 0xF) {
    // Rt == PC
    uint32_t ry;  // r0 - r4
    for (ry = 4;; --ry)
      if (ry != rt && ry != rt2 && ry != rm && ry != rx) break;

    buf[0] = 0x0A000000u | (cond << 28u);           // B<c> #0
    buf[1] = 0xEA000006;                            // B #24
    buf[2] = 0xE92D8000 | (1u << rx) | (1u << ry);  // PUSH {Rx, Ry, PC}
    buf[3] = 0xE59F0000 | (rx << 12u);              // LDR Rx, [PC, #8]
    buf[4] = 0xEA000000;                            // B #0
    buf[5] = pc;
    buf[6] =
        (inst & 0x0FF00FFFu) | 0xE0000000 | (rx << 16u) | (ry << 12u);  // LDRxx Ry, [Rx],+/-Rm{, <shift>}
    buf[7] = 0xE58D0008 | (ry << 12u);                                  // STR Ry, [SP, #8]
    buf[8] = 0xE8BD8000 | (1u << rx) | (1u << ry);                      // POP {Rx, Ry, PC}
    return 36;
  } else {
    buf[0] = 0x0A000000u | (cond << 28u);  // B<c> #0
    buf[1] = 0xEA000005;                   // B #20
    buf[2] = 0xE52D0004 | (rx << 12u);     // PUSH {Rx}
    buf[3] = 0xE59F0000 | (rx << 12u);     // LDR Rx, [PC, #0]
    buf[4] = 0xEA000000;                   // B #0
    buf[5] = pc;
    buf[6] = (inst & 0x0FF0FFFFu) | 0xE0000000 | (rx << 16u);  // LDRxx Rt, [Rx],+/-Rm{, <shift>}
    buf[7] = 0xE49D0004 | (rx << 12u);                         // POP {Rx}
    return 32;
  }
}

size_t sh_a32_rewrite(uint32_t *buf, uint32_t inst, uintptr_t pc, sh_a32_rewrite_info_t *rinfo) {
  sh_a32_type_t type = sh_a32_get_type(inst);
  SH_LOG_INFO("a32 rewrite: type %d, inst %" PRIx32, type, inst);

  // We will only overwrite 4 to 8 bytes on A32, so PC cannot be in the coverage.
  // In this case, the add/sub/mov/ldr_reg instruction does not need to consider
  // the problem of PC in the coverage area when rewriting.

  if (type == B_A1 || type == BX_A1 || type == BL_IMM_A1 || type == BLX_IMM_A2)
    return sh_a32_rewrite_b(buf, inst, pc, type, rinfo);
  else if (type == ADD_REG_A1 || type == ADD_REG_PC_A1 || type == SUB_REG_A1 || type == SUB_REG_PC_A1)
    return sh_a32_rewrite_add_or_sub(buf, inst, pc);
  else if (type == ADR_A1 || type == ADR_A2)
    return sh_a32_rewrite_adr(buf, inst, pc, type, rinfo);
  else if (type == MOV_REG_A1 || type == MOV_REG_PC_A1)
    return sh_a32_rewrite_mov(buf, inst, pc);
  else if (type == LDR_LIT_A1 || type == LDR_LIT_PC_A1 || type == LDRB_LIT_A1 || type == LDRD_LIT_A1 ||
           type == LDRH_LIT_A1 || type == LDRSB_LIT_A1 || type == LDRSH_LIT_A1)
    return sh_a32_rewrite_ldr_lit(buf, inst, pc, type, rinfo);
  else if (type == LDR_REG_A1 || type == LDR_REG_PC_A1 || type == LDRB_REG_A1 || type == LDRD_REG_A1 ||
           type == LDRH_REG_A1 || type == LDRSB_REG_A1 || type == LDRSH_REG_A1)
    return sh_a32_rewrite_ldr_reg(buf, inst, pc, type);
  else {
    // IGNORED
    buf[0] = inst;
    return 4;
  }
}

size_t sh_a32_absolute_jump(uint32_t *buf, uintptr_t addr) {
  buf[0] = 0xE51FF004;  // LDR PC, [PC, #-4]
  buf[1] = addr;
  return 8;
}

size_t sh_a32_relative_jump(uint32_t *buf, uintptr_t addr, uintptr_t pc) {
  buf[0] = 0xEA000000 | (((addr - pc) & 0x03FFFFFFu) >> 2u);  // B <label>
  return 4;
}
