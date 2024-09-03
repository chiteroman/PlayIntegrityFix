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

#include "sh_a64.h"

#include <inttypes.h>
#include <sh_util.h>
#include <stdint.h>

#include "sh_log.h"

// https://developer.arm.com/documentation/ddi0487/latest
// https://developer.arm.com/documentation/ddi0602/latest

typedef enum {
  IGNORED = 0,
  B,
  B_COND,
  BL,
  ADR,
  ADRP,
  LDR_LIT_32,
  LDR_LIT_64,
  LDRSW_LIT,
  PRFM_LIT,
  LDR_SIMD_LIT_32,
  LDR_SIMD_LIT_64,
  LDR_SIMD_LIT_128,
  CBZ,
  CBNZ,
  TBZ,
  TBNZ
} sh_a64_type_t;

static sh_a64_type_t sh_a64_get_type(uint32_t inst) {
  if ((inst & 0xFC000000) == 0x14000000)
    return B;
  else if ((inst & 0xFF000010) == 0x54000000)
    return B_COND;
  else if ((inst & 0xFC000000) == 0x94000000)
    return BL;
  else if ((inst & 0x9F000000) == 0x10000000)
    return ADR;
  else if ((inst & 0x9F000000) == 0x90000000)
    return ADRP;
  else if ((inst & 0xFF000000) == 0x18000000)
    return LDR_LIT_32;
  else if ((inst & 0xFF000000) == 0x58000000)
    return LDR_LIT_64;
  else if ((inst & 0xFF000000) == 0x98000000)
    return LDRSW_LIT;
  else if ((inst & 0xFF000000) == 0xD8000000)
    return PRFM_LIT;
  else if ((inst & 0xFF000000) == 0x1C000000)
    return LDR_SIMD_LIT_32;
  else if ((inst & 0xFF000000) == 0x5C000000)
    return LDR_SIMD_LIT_64;
  else if ((inst & 0xFF000000) == 0x9C000000)
    return LDR_SIMD_LIT_128;
  else if ((inst & 0x7F000000u) == 0x34000000)
    return CBZ;
  else if ((inst & 0x7F000000u) == 0x35000000)
    return CBNZ;
  else if ((inst & 0x7F000000u) == 0x36000000)
    return TBZ;
  else if ((inst & 0x7F000000u) == 0x37000000)
    return TBNZ;
  else
    return IGNORED;
}

size_t sh_a64_get_rewrite_inst_len(uint32_t inst) {
  static uint8_t map[] = {
      4,   // IGNORED
      20,  // B
      28,  // B_COND
      20,  // BL
      16,  // ADR
      16,  // ADRP
      20,  // LDR_LIT_32
      20,  // LDR_LIT_64
      20,  // LDRSW_LIT
      28,  // PRFM_LIT
      28,  // LDR_SIMD_LIT_32
      28,  // LDR_SIMD_LIT_64
      28,  // LDR_SIMD_LIT_128
      24,  // CBZ
      24,  // CBNZ
      24,  // TBZ
      24   // TBNZ
  };

  return (size_t)(map[sh_a64_get_type(inst)]);
}

static bool sh_a64_is_addr_need_fix(uintptr_t addr, sh_a64_rewrite_info_t *rinfo) {
  return (rinfo->start_addr <= addr && addr < rinfo->end_addr);
}

static uintptr_t sh_a64_fix_addr(uintptr_t addr, sh_a64_rewrite_info_t *rinfo) {
  if (rinfo->start_addr <= addr && addr < rinfo->end_addr) {
    uintptr_t cursor_addr = rinfo->start_addr;
    size_t offset = 0;
    for (size_t i = 0; i < rinfo->inst_lens_cnt; i++) {
      if (cursor_addr >= addr) break;
      cursor_addr += 4;
      offset += rinfo->inst_lens[i];
    }
    uintptr_t fixed_addr = (uintptr_t)rinfo->buf + offset;
    SH_LOG_INFO("a64 rewrite: fix addr %" PRIxPTR " -> %" PRIxPTR, addr, fixed_addr);
    return fixed_addr;
  }

  return addr;
}

static size_t sh_a64_rewrite_b(uint32_t *buf, uint32_t inst, uintptr_t pc, sh_a64_type_t type,
                               sh_a64_rewrite_info_t *rinfo) {
  uint64_t imm64;
  if (type == B_COND) {
    uint64_t imm19 = SH_UTIL_GET_BITS_32(inst, 23, 5);
    imm64 = SH_UTIL_SIGN_EXTEND_64(imm19 << 2u, 21u);
  } else {
    uint64_t imm26 = SH_UTIL_GET_BITS_32(inst, 25, 0);
    imm64 = SH_UTIL_SIGN_EXTEND_64(imm26 << 2u, 28u);
  }
  uint64_t addr = pc + imm64;
  addr = sh_a64_fix_addr(addr, rinfo);

  size_t idx = 0;
  if (type == B_COND) {
    buf[idx++] = (inst & 0xFF00001F) | 0x40u;  // B.<cond> #8
    buf[idx++] = 0x14000006;                   // B #24
  }
  buf[idx++] = 0x58000051;  // LDR X17, #8
  buf[idx++] = 0x14000003;  // B #12
  buf[idx++] = addr & 0xFFFFFFFF;
  buf[idx++] = addr >> 32u;
  if (type == BL)
    buf[idx++] = 0xD63F0220;  // BLR X17
  else
    buf[idx++] = 0xD61F0220;  // BR X17
  return idx * 4;             // 20 or 28
}

static size_t sh_a64_rewrite_adr(uint32_t *buf, uint32_t inst, uintptr_t pc, sh_a64_type_t type,
                                 sh_a64_rewrite_info_t *rinfo) {
  uint32_t xd = SH_UTIL_GET_BITS_32(inst, 4, 0);
  uint64_t immlo = SH_UTIL_GET_BITS_32(inst, 30, 29);
  uint64_t immhi = SH_UTIL_GET_BITS_32(inst, 23, 5);
  uint64_t addr;
  if (type == ADR)
    addr = pc + SH_UTIL_SIGN_EXTEND_64((immhi << 2u) | immlo, 21u);
  else  // ADRP
    addr = (pc & 0xFFFFFFFFFFFFF000) + SH_UTIL_SIGN_EXTEND_64((immhi << 14u) | (immlo << 12u), 33u);
  if (sh_a64_is_addr_need_fix(addr, rinfo)) return 0;  // rewrite failed

  buf[0] = 0x58000040u | xd;  // LDR Xd, #8
  buf[1] = 0x14000003;        // B #12
  buf[2] = addr & 0xFFFFFFFF;
  buf[3] = addr >> 32u;
  return 16;
}

static size_t sh_a64_rewrite_ldr(uint32_t *buf, uint32_t inst, uintptr_t pc, sh_a64_type_t type,
                                 sh_a64_rewrite_info_t *rinfo) {
  uint32_t rt = SH_UTIL_GET_BITS_32(inst, 4, 0);
  uint64_t imm19 = SH_UTIL_GET_BITS_32(inst, 23, 5);
  uint64_t offset = SH_UTIL_SIGN_EXTEND_64((imm19 << 2u), 21u);
  uint64_t addr = pc + offset;

  if (sh_a64_is_addr_need_fix(addr, rinfo)) {
    if (type != PRFM_LIT) return 0;  // rewrite failed
    addr = sh_a64_fix_addr(addr, rinfo);
  }

  if (type == LDR_LIT_32 || type == LDR_LIT_64 || type == LDRSW_LIT) {
    buf[0] = 0x58000060u | rt;  // LDR Xt, #12
    if (type == LDR_LIT_32)
      buf[1] = 0xB9400000 | rt | (rt << 5u);  // LDR Wt, [Xt]
    else if (type == LDR_LIT_64)
      buf[1] = 0xF9400000 | rt | (rt << 5u);  // LDR Xt, [Xt]
    else
      // LDRSW_LIT
      buf[1] = 0xB9800000 | rt | (rt << 5u);  // LDRSW Xt, [Xt]
    buf[2] = 0x14000003;                      // B #12
    buf[3] = addr & 0xFFFFFFFF;
    buf[4] = addr >> 32u;
    return 20;
  } else {
    buf[0] = 0xA93F47F0;  // STP X16, X17, [SP, -0x10]
    buf[1] = 0x58000091;  // LDR X17, #16
    if (type == PRFM_LIT)
      buf[2] = 0xF9800220 | rt;  // PRFM Rt, [X17]
    else if (type == LDR_SIMD_LIT_32)
      buf[2] = 0xBD400220 | rt;  // LDR St, [X17]
    else if (type == LDR_SIMD_LIT_64)
      buf[2] = 0xFD400220 | rt;  // LDR Dt, [X17]
    else
      // LDR_SIMD_LIT_128
      buf[2] = 0x3DC00220u | rt;  // LDR Qt, [X17]
    buf[3] = 0xF85F83F1;          // LDR X17, [SP, -0x8]
    buf[4] = 0x14000003;          // B #12
    buf[5] = addr & 0xFFFFFFFF;
    buf[6] = addr >> 32u;
    return 28;
  }
}

static size_t sh_a64_rewrite_cb(uint32_t *buf, uint32_t inst, uintptr_t pc, sh_a64_rewrite_info_t *rinfo) {
  uint64_t imm19 = SH_UTIL_GET_BITS_32(inst, 23, 5);
  uint64_t offset = SH_UTIL_SIGN_EXTEND_64((imm19 << 2u), 21u);
  uint64_t addr = pc + offset;
  addr = sh_a64_fix_addr(addr, rinfo);

  buf[0] = (inst & 0xFF00001F) | 0x40u;  // CB(N)Z Rt, #8
  buf[1] = 0x14000005;                   // B #20
  buf[2] = 0x58000051;                   // LDR X17, #8
  buf[3] = 0xd61f0220;                   // BR X17
  buf[4] = addr & 0xFFFFFFFF;
  buf[5] = addr >> 32u;
  return 24;
}

static size_t sh_a64_rewrite_tb(uint32_t *buf, uint32_t inst, uintptr_t pc, sh_a64_rewrite_info_t *rinfo) {
  uint64_t imm14 = SH_UTIL_GET_BITS_32(inst, 18, 5);
  uint64_t offset = SH_UTIL_SIGN_EXTEND_64((imm14 << 2u), 16u);
  uint64_t addr = pc + offset;
  addr = sh_a64_fix_addr(addr, rinfo);

  buf[0] = (inst & 0xFFF8001F) | 0x40u;  // TB(N)Z Rt, #<imm>, #8
  buf[1] = 0x14000005;                   // B #20
  buf[2] = 0x58000051;                   // LDR X17, #8
  buf[3] = 0xd61f0220;                   // BR X17
  buf[4] = addr & 0xFFFFFFFF;
  buf[5] = addr >> 32u;
  return 24;
}

size_t sh_a64_rewrite(uint32_t *buf, uint32_t inst, uintptr_t pc, sh_a64_rewrite_info_t *rinfo) {
  sh_a64_type_t type = sh_a64_get_type(inst);
  SH_LOG_INFO("a64 rewrite: type %d, inst %" PRIx32, type, inst);

  if (type == B || type == B_COND || type == BL)
    return sh_a64_rewrite_b(buf, inst, pc, type, rinfo);
  else if (type == ADR || type == ADRP)
    return sh_a64_rewrite_adr(buf, inst, pc, type, rinfo);
  else if (type == LDR_LIT_32 || type == LDR_LIT_64 || type == LDRSW_LIT || type == PRFM_LIT ||
           type == LDR_SIMD_LIT_32 || type == LDR_SIMD_LIT_64 || type == LDR_SIMD_LIT_128)
    return sh_a64_rewrite_ldr(buf, inst, pc, type, rinfo);
  else if (type == CBZ || type == CBNZ)
    return sh_a64_rewrite_cb(buf, inst, pc, rinfo);
  else if (type == TBZ || type == TBNZ)
    return sh_a64_rewrite_tb(buf, inst, pc, rinfo);
  else {
    // IGNORED
    buf[0] = inst;
    return 4;
  }
}

size_t sh_a64_absolute_jump_with_br(uint32_t *buf, uintptr_t addr) {
  buf[0] = 0x58000051;  // LDR X17, #8
  buf[1] = 0xd61f0220;  // BR X17
  buf[2] = addr & 0xFFFFFFFF;
  buf[3] = addr >> 32u;
  return 16;
}

// Use RET instead of BR to bypass arm64 BTI.
//
// ref:
// https://developer.arm.com/documentation/102433/0100/Jump-oriented-programming
// https://developer.arm.com/documentation/ddi0602/2023-06/Base-Instructions/BTI--Branch-Target-Identification-?lang=en
// https://github.com/torvalds/linux/commit/8ef8f360cf30be12382f89ff48a57fbbd9b31c14
// https://android-review.googlesource.com/c/platform/bionic/+/1242754
// https://developer.android.com/ndk/guides/abis#armv9_enabling_pac_and_bti_for_cc
// https://developer.arm.com/documentation/100067/0612/armclang-Command-line-Options/-mbranch-protection
size_t sh_a64_absolute_jump_with_ret(uint32_t *buf, uintptr_t addr) {
  buf[0] = 0x58000051;  // LDR X17, #8
  buf[1] = 0xd65f0220;  // RET X17
  buf[2] = addr & 0xFFFFFFFF;
  buf[3] = addr >> 32u;
  return 16;
}

size_t sh_a64_relative_jump(uint32_t *buf, uintptr_t addr, uintptr_t pc) {
  buf[0] = 0x14000000u | (((addr - pc) & 0x0FFFFFFFu) >> 2u);  // B <label>
  return 4;
}
