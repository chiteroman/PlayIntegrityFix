// Copyright (c) 2021-2022 ByteDance Inc.
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

// Created by Pengying Xu (xupengying@bytedance.com) on 2021-04-11.

#include "sh_t16.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "sh_log.h"
#include "sh_util.h"

// https://developer.arm.com/documentation/ddi0406/latest
// https://developer.arm.com/documentation/ddi0597/latest

typedef enum {
  IGNORED = 0,
  IT_T1,
  B_T1,
  B_T2,
  BX_T1,
  ADD_REG_T2,
  MOV_REG_T1,
  ADR_T1,
  LDR_LIT_T1,
  CBZ_T1,
  CBNZ_T1
} sh_t16_type_t;

static sh_t16_type_t sh_t16_get_type(uint16_t inst) {
  if (((inst & 0xFF00u) == 0xBF00) && ((inst & 0x000Fu) != 0x0000) && ((inst & 0x00F0u) != 0x00F0))
    return IT_T1;
  else if (((inst & 0xF000u) == 0xD000) && ((inst & 0x0F00u) != 0x0F00) && ((inst & 0x0F00u) != 0x0E00))
    return B_T1;
  else if ((inst & 0xF800u) == 0xE000)
    return B_T2;
  else if ((inst & 0xFFF8u) == 0x4778)
    return BX_T1;
  else if (((inst & 0xFF78u) == 0x4478) && ((inst & 0x0087u) != 0x0085))
    return ADD_REG_T2;
  else if ((inst & 0xFF78u) == 0x4678)
    return MOV_REG_T1;
  else if ((inst & 0xF800u) == 0xA000)
    return ADR_T1;
  else if ((inst & 0xF800u) == 0x4800)
    return LDR_LIT_T1;
  else if ((inst & 0xFD00u) == 0xB100)
    return CBZ_T1;
  else if ((inst & 0xFD00u) == 0xB900)
    return CBNZ_T1;
  else
    return IGNORED;
}

size_t sh_t16_get_rewrite_inst_len(uint16_t inst) {
  static uint8_t map[] = {
      4,   // IGNORED
      0,   // IT_T1
      12,  // B_T1
      8,   // B_T2
      8,   // BX_T1
      16,  // ADD_REG_T2
      12,  // MOV_REG_T1
      8,   // ADR_T1
      12,  // LDR_LIT_T1
      12,  // CBZ_T1
      12   // CBNZ_T1
  };

  return (size_t)(map[sh_t16_get_type(inst)]);
}

static size_t sh_t16_rewrite_b(uint16_t *buf, uint16_t inst, uintptr_t pc, sh_t16_type_t type,
                               sh_txx_rewrite_info_t *rinfo) {
  uint32_t addr;
  if (type == B_T1) {
    uint32_t imm8 = SH_UTIL_GET_BITS_16(inst, 7, 0);
    addr = pc + SH_UTIL_SIGN_EXTEND_32(imm8 << 1u, 9u);
    addr = SH_UTIL_SET_BIT0(addr);  // thumb -> thumb
  } else if (type == B_T2) {
    uint32_t imm11 = SH_UTIL_GET_BITS_16(inst, 10, 0);
    addr = pc + SH_UTIL_SIGN_EXTEND_32(imm11 << 1u, 12u);
    addr = SH_UTIL_SET_BIT0(addr);  // thumb -> thumb
  } else {
    // type == BX_T1
    // BX PC
    // PC must be even, and the "BX PC" instruction must be at a 4-byte aligned address,
    // so the instruction set must be exchanged from "thumb" to "arm".
    addr = pc;  // thumb -> arm
  }
  addr = sh_txx_fix_addr(addr, rinfo);

  size_t idx = 0;
  if (type == B_T1) {
    buf[idx++] = inst & 0xFF00u;  // B<c> #0
    buf[idx++] = 0xE003;          // B PC, #6
  }
  buf[idx++] = 0xF8DF;  // LDR.W PC, [PC]
  buf[idx++] = 0xF000;  // ...
  buf[idx++] = addr & 0xFFFFu;
  buf[idx++] = addr >> 16u;
  return idx * 2;  // 8 or 12
}

static size_t sh_t16_rewrite_add(uint16_t *buf, uint16_t inst, uintptr_t pc) {
  // ADD<c> <Rdn>, PC
  uint16_t dn = SH_UTIL_GET_BIT_16(inst, 7);
  uint16_t rdn = SH_UTIL_GET_BITS_16(inst, 2, 0);
  uint16_t rd = (uint16_t)(dn << 3u) | rdn;
  uint16_t rx = (rd == 0) ? 1 : 0;  // r0 - r1

  buf[0] = (uint16_t)(0xB400u | (1u << rx));         // PUSH {Rx}
  buf[1] = 0x4802u | (uint16_t)(rx << 8u);           // LDR Rx, [PC, #8]
  buf[2] = (inst & 0xFF87u) | (uint16_t)(rx << 3u);  // ADD Rd, Rx
  buf[3] = (uint16_t)(0xBC00u | (1u << rx));         // POP {Rx}
  buf[4] = 0xE002;                                   // B #4
  buf[5] = 0xBF00;
  buf[6] = pc & 0xFFFFu;
  buf[7] = pc >> 16u;
  return 16;
}

static size_t sh_t16_rewrite_mov(uint16_t *buf, uint16_t inst, uintptr_t pc) {
  // MOV<c> <Rd>, PC
  uint16_t D = SH_UTIL_GET_BIT_16(inst, 7);
  uint16_t rd = SH_UTIL_GET_BITS_16(inst, 2, 0);
  uint16_t d = (uint16_t)(D << 3u) | rd;  // r0 - r15

  buf[0] = 0xF8DF;                     // LDR.W Rd, [PC, #4]
  buf[1] = (uint16_t)(d << 12u) + 4u;  // ...
  buf[2] = 0xE002;                     // B #4
  buf[3] = 0xBF00;                     // NOP
  buf[4] = pc & 0xFFFFu;
  buf[5] = pc >> 16u;
  return 12;
}

static size_t sh_t16_rewrite_adr(uint16_t *buf, uint16_t inst, uintptr_t pc, sh_txx_rewrite_info_t *rinfo) {
  // ADR<c> <Rd>, <label>
  uint16_t rd = SH_UTIL_GET_BITS_16(inst, 10, 8);  // r0 - r7
  uint16_t imm8 = SH_UTIL_GET_BITS_16(inst, 7, 0);
  uint32_t addr = SH_UTIL_ALIGN_4(pc) + (uint32_t)(imm8 << 2u);
  if (sh_txx_is_addr_need_fix(addr, rinfo)) return 0;  // rewrite failed

  buf[0] = 0x4800u | (uint16_t)(rd << 8u);  // LDR Rd, [PC]
  buf[1] = 0xE001;                          // B #2
  buf[2] = addr & 0xFFFFu;
  buf[3] = addr >> 16u;
  return 8;
}

static size_t sh_t16_rewrite_ldr(uint16_t *buf, uint16_t inst, uintptr_t pc, sh_txx_rewrite_info_t *rinfo) {
  // LDR<c> <Rt>, <label>
  uint16_t rt = SH_UTIL_GET_BITS_16(inst, 10, 8);  // r0 - r7
  uint16_t imm8 = SH_UTIL_GET_BITS_16(inst, 7, 0);
  uint32_t addr = SH_UTIL_ALIGN_4(pc) + (uint32_t)(imm8 << 2u);
  if (sh_txx_is_addr_need_fix(addr, rinfo)) return 0;  // rewrite failed

  buf[0] = 0x4800u | (uint16_t)(rt << 8u);  // LDR Rt, [PC]
  buf[1] = 0xE001;                          // B #2
  buf[2] = addr & 0xFFFFu;
  buf[3] = addr >> 16u;
  buf[4] = 0x6800u | (uint16_t)(rt << 3u) | rt;  // LDR Rt, [Rt]
  buf[5] = 0xBF00;                               // NOP
  return 12;
}

static size_t sh_t16_rewrite_cb(uint16_t *buf, uint16_t inst, uintptr_t pc, sh_txx_rewrite_info_t *rinfo) {
  // CB{N}Z <Rn>, <label>
  uint16_t i = SH_UTIL_GET_BIT_16(inst, 9);
  uint16_t imm5 = SH_UTIL_GET_BITS_16(inst, 7, 3);
  uint32_t imm32 = (uint32_t)(i << 6u) | (uint32_t)(imm5 << 1u);
  uint32_t addr = SH_UTIL_SET_BIT0(pc + imm32);  // thumb -> thumb
  addr = sh_txx_fix_addr(addr, rinfo);

  buf[0] = inst & 0xFD07u;  // CB(N)Z Rn, #0
  buf[1] = 0xE003;          // B PC, #6
  buf[2] = 0xF8DF;          // LDR.W PC, [PC]
  buf[3] = 0xF000;          // ...
  buf[4] = addr & 0xFFFFu;
  buf[5] = addr >> 16u;
  return 12;
}

size_t sh_t16_rewrite(uint16_t *buf, uint16_t inst, uintptr_t pc, sh_txx_rewrite_info_t *rinfo) {
  sh_t16_type_t type = sh_t16_get_type(inst);
  SH_LOG_INFO("t16 rewrite: type %d, inst %" PRIx16, type, inst);

  if (type == B_T1 || type == B_T2 || type == BX_T1)
    return sh_t16_rewrite_b(buf, inst, pc, type, rinfo);
  else if (type == ADD_REG_T2)
    return sh_t16_rewrite_add(buf, inst, pc);
  else if (type == MOV_REG_T1)
    return sh_t16_rewrite_mov(buf, inst, pc);
  else if (type == ADR_T1)
    return sh_t16_rewrite_adr(buf, inst, pc, rinfo);
  else if (type == LDR_LIT_T1)
    return sh_t16_rewrite_ldr(buf, inst, pc, rinfo);
  else if (type == CBZ_T1 || type == CBNZ_T1)
    return sh_t16_rewrite_cb(buf, inst, pc, rinfo);
  else {
    // IGNORED
    buf[0] = inst;
    buf[1] = 0xBF00;  // NOP
    return 4;
  }
}

static size_t sh_t16_get_it_insts_count(uint16_t inst) {
  if ((inst & 0x1u) != 0) return 4;
  if ((inst & 0x2u) != 0) return 3;
  if ((inst & 0x4u) != 0) return 2;
  return 1;
}

bool sh_t16_parse_it(sh_t16_it_t *it, uint16_t inst, uintptr_t pc) {
  if (IT_T1 != sh_t16_get_type(inst)) return false;
  SH_LOG_INFO("t16 rewrite: type IT, inst %" PRIx16, inst);

  // address of the first inst in the IT block, skip the IT inst itself (2 bytes)
  uintptr_t target_addr = pc - 4 + 2;

  it->firstcond = (uint8_t)(inst >> 4u);
  uint8_t firstcond_0 = it->firstcond & 1u;

  memset(it, 0, sizeof(sh_t16_it_t));
  it->insts_cnt = sh_t16_get_it_insts_count(inst);

  size_t insts_idx = 0, pcs_idx = 0;
  for (int parse_else = 1; parse_else >= 0; parse_else--)  // round 0: parse ELSE, round 1: THEN
  {
    uintptr_t target_offset = 0;
    for (size_t i = 0; i < it->insts_cnt; i++) {
      bool is_thumb32 = sh_util_is_thumb32(target_addr + target_offset);
      uint8_t mask_x = (uint8_t)(inst >> (uint16_t)(4 - i)) & 1u;

      if ((parse_else && mask_x != firstcond_0) ||  // parse ELSE or
          (!parse_else && mask_x == firstcond_0))   // parse THEN
      {
        it->insts[insts_idx++] = *((uint16_t *)(target_addr + target_offset));
        if (is_thumb32) it->insts[insts_idx++] = *((uint16_t *)(target_addr + target_offset + 2));

        it->pcs[pcs_idx++] = target_addr + target_offset + 4;
        if (parse_else) it->insts_else_cnt++;
      }

      target_offset += (is_thumb32 ? 4 : 2);
    }
  }
  it->insts_len = insts_idx * 2;

  return true;
}

void sh_t16_rewrite_it_else(uint16_t *buf, uint16_t imm9, sh_t16_it_t *it) {
  buf[0] = 0xD000u | (uint16_t)(it->firstcond << 8u) | (uint16_t)(imm9 >> 1u);  // B<c> <label>
  buf[1] = 0xBF00;                                                              // NOP
}

void sh_t16_rewrite_it_then(uint16_t *buf, uint16_t imm12) {
  buf[0] = 0xE000u | (uint16_t)(imm12 >> 1u);  // B <label>
  buf[1] = 0xBF00;                             // NOP
}
