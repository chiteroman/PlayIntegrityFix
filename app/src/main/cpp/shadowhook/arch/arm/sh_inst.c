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

// Created by Kelun Cai (caikelun@bytedance.com) on 2021-04-11.

#include "sh_inst.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "sh_a32.h"
#include "sh_config.h"
#include "sh_enter.h"
#include "sh_exit.h"
#include "sh_log.h"
#include "sh_sig.h"
#include "sh_t16.h"
#include "sh_t32.h"
#include "sh_txx.h"
#include "sh_util.h"
#include "shadowhook.h"

static void sh_inst_get_thumb_rewrite_info(sh_inst_t *self, uintptr_t target_addr,
                                           sh_txx_rewrite_info_t *rinfo) {
  memset(rinfo, 0, sizeof(sh_txx_rewrite_info_t));

  size_t idx = 0;
  uintptr_t target_addr_offset = 0;
  uintptr_t pc = target_addr + 4;
  size_t rewrite_len = 0;

  while (rewrite_len < self->backup_len) {
    // IT block
    sh_t16_it_t it;
    if (sh_t16_parse_it(&it, *((uint16_t *)(target_addr + target_addr_offset)), pc)) {
      rewrite_len += (2 + it.insts_len);

      size_t it_block_idx = idx++;
      size_t it_block_len = 4 + 4;  // IT-else + IT-then
      for (size_t i = 0, j = 0; i < it.insts_cnt; i++) {
        bool is_thumb32 = sh_util_is_thumb32((uintptr_t)(&(it.insts[j])));
        if (is_thumb32) {
          it_block_len += sh_t32_get_rewrite_inst_len(it.insts[j], it.insts[j + 1]);
          rinfo->inst_lens[idx++] = 0;
          rinfo->inst_lens[idx++] = 0;
          j += 2;
        } else {
          it_block_len += sh_t16_get_rewrite_inst_len(it.insts[j]);
          rinfo->inst_lens[idx++] = 0;
          j += 1;
        }
      }
      rinfo->inst_lens[it_block_idx] = it_block_len;

      target_addr_offset += (2 + it.insts_len);
      pc += (2 + it.insts_len);
    }
    // not IT block
    else {
      bool is_thumb32 = sh_util_is_thumb32(target_addr + target_addr_offset);
      size_t inst_len = (is_thumb32 ? 4 : 2);
      rewrite_len += inst_len;

      if (is_thumb32) {
        rinfo->inst_lens[idx++] =
            sh_t32_get_rewrite_inst_len(*((uint16_t *)(target_addr + target_addr_offset)),
                                        *((uint16_t *)(target_addr + target_addr_offset + 2)));
        rinfo->inst_lens[idx++] = 0;
      } else
        rinfo->inst_lens[idx++] =
            sh_t16_get_rewrite_inst_len(*((uint16_t *)(target_addr + target_addr_offset)));

      target_addr_offset += inst_len;
      pc += inst_len;
    }
  }

  rinfo->start_addr = target_addr;
  rinfo->end_addr = target_addr + rewrite_len;
  rinfo->buf = (uint16_t *)self->enter_addr;
  rinfo->buf_offset = 0;
  rinfo->inst_lens_cnt = idx;
}

static int sh_inst_hook_thumb_rewrite(sh_inst_t *self, uintptr_t target_addr, uintptr_t *orig_addr,
                                      uintptr_t *orig_addr2, size_t *rewrite_len) {
  // backup original instructions (length: 4 or 8 or 10)
  memcpy((void *)(self->backup), (void *)target_addr, self->backup_len);

  // package the information passed to rewrite
  sh_txx_rewrite_info_t rinfo;
  sh_inst_get_thumb_rewrite_info(self, target_addr, &rinfo);

  // backup and rewrite original instructions
  uintptr_t target_addr_offset = 0;
  uintptr_t pc = target_addr + 4;
  *rewrite_len = 0;
  while (*rewrite_len < self->backup_len) {
    // IT block
    sh_t16_it_t it;
    if (sh_t16_parse_it(&it, *((uint16_t *)(target_addr + target_addr_offset)), pc)) {
      *rewrite_len += (2 + it.insts_len);

      // save space holder point of IT-else B instruction
      uintptr_t enter_inst_else_p = self->enter_addr + rinfo.buf_offset;
      rinfo.buf_offset += 2;  // B<c> <label>
      rinfo.buf_offset += 2;  // NOP

      // rewrite IT block
      size_t enter_inst_else_len = 4;  // B<c> + NOP + B + NOP
      size_t enter_inst_then_len = 0;  // B + NOP
      uintptr_t enter_inst_then_p = 0;
      for (size_t i = 0, j = 0; i < it.insts_cnt; i++) {
        if (i == it.insts_else_cnt) {
          // save space holder point of IT-then (for B instruction)
          enter_inst_then_p = self->enter_addr + rinfo.buf_offset;
          rinfo.buf_offset += 2;  // B <label>
          rinfo.buf_offset += 2;  // NOP

          // fill IT-else B instruction
          sh_t16_rewrite_it_else((uint16_t *)enter_inst_else_p, (uint16_t)enter_inst_else_len, &it);
        }

        // rewrite instructions in IT block
        bool is_thumb32 = sh_util_is_thumb32((uintptr_t)(&(it.insts[j])));
        size_t len;
        if (is_thumb32)
          len = sh_t32_rewrite((uint16_t *)(self->enter_addr + rinfo.buf_offset), it.insts[j],
                               it.insts[j + 1], it.pcs[i], &rinfo);
        else
          len = sh_t16_rewrite((uint16_t *)(self->enter_addr + rinfo.buf_offset), it.insts[j], it.pcs[i],
                               &rinfo);
        if (0 == len) return SHADOWHOOK_ERRNO_HOOK_REWRITE_FAILED;
        rinfo.buf_offset += len;
        j += (is_thumb32 ? 2 : 1);

        // save the total offset for ELSE/THEN in enter
        if (i < it.insts_else_cnt)
          enter_inst_else_len += len;
        else
          enter_inst_then_len += len;

        if (i == it.insts_cnt - 1) {
          // fill IT-then B instruction
          sh_t16_rewrite_it_then((uint16_t *)enter_inst_then_p, (uint16_t)enter_inst_then_len);
        }
      }

      target_addr_offset += (2 + it.insts_len);
      pc += (2 + it.insts_len);
    }
    // not IT block
    else {
      bool is_thumb32 = sh_util_is_thumb32(target_addr + target_addr_offset);
      size_t inst_len = (is_thumb32 ? 4 : 2);
      *rewrite_len += inst_len;

      // rewrite original instructions (fill in enter)
      SH_LOG_INFO("thumb rewrite: offset %zu, pc %" PRIxPTR, rinfo.buf_offset, pc);
      size_t len;
      if (is_thumb32)
        len = sh_t32_rewrite((uint16_t *)(self->enter_addr + rinfo.buf_offset),
                             *((uint16_t *)(target_addr + target_addr_offset)),
                             *((uint16_t *)(target_addr + target_addr_offset + 2)), pc, &rinfo);
      else
        len = sh_t16_rewrite((uint16_t *)(self->enter_addr + rinfo.buf_offset),
                             *((uint16_t *)(target_addr + target_addr_offset)), pc, &rinfo);
      if (0 == len) return SHADOWHOOK_ERRNO_HOOK_REWRITE_FAILED;
      rinfo.buf_offset += len;

      target_addr_offset += inst_len;
      pc += inst_len;
    }
  }
  SH_LOG_INFO("thumb rewrite: len %zu to %zu", *rewrite_len, rinfo.buf_offset);

  // absolute jump back to remaining original instructions (fill in enter)
  rinfo.buf_offset += sh_t32_absolute_jump((uint16_t *)(self->enter_addr + rinfo.buf_offset), true,
                                           SH_UTIL_SET_BIT0(target_addr + *rewrite_len));
  sh_util_clear_cache(self->enter_addr, rinfo.buf_offset);

  // save original function address
  if (NULL != orig_addr) __atomic_store_n(orig_addr, SH_UTIL_SET_BIT0(self->enter_addr), __ATOMIC_SEQ_CST);
  if (NULL != orig_addr2) __atomic_store_n(orig_addr2, SH_UTIL_SET_BIT0(self->enter_addr), __ATOMIC_SEQ_CST);
  return 0;
}

#ifdef SH_CONFIG_DETECT_THUMB_TAIL_ALIGNED
static bool sh_inst_thumb_is_long_enough(uintptr_t target_addr, size_t overwrite_len, xdl_info_t *dlinfo) {
  if (overwrite_len <= dlinfo->dli_ssize) return true;

  // check align-4 in the end of symbol
  if ((overwrite_len == dlinfo->dli_ssize + 2) && ((target_addr + dlinfo->dli_ssize) % 4 == 2)) {
    uintptr_t sym_end = target_addr + dlinfo->dli_ssize;
    if (0 != sh_util_mprotect(sym_end, 2, PROT_READ | PROT_WRITE | PROT_EXEC)) return false;

    // should be zero-ed
    if (0 != *((uint16_t *)sym_end)) return false;

    // should not belong to any symbol
    void *dlcache = NULL;
    xdl_info_t dlinfo2;
    if (sh_util_get_api_level() >= __ANDROID_API_L__) {
      xdl_addr((void *)SH_UTIL_SET_BIT0(sym_end), &dlinfo2, &dlcache);
    } else {
      SH_SIG_TRY(SIGSEGV, SIGBUS) {
        xdl_addr((void *)SH_UTIL_SET_BIT0(sym_end), &dlinfo2, &dlcache);
      }
      SH_SIG_CATCH() {
        memset(&dlinfo2, 0, sizeof(dlinfo2));
        SH_LOG_WARN("thumb detect tail aligned: crashed");
      }
      SH_SIG_EXIT
    }
    xdl_addr_clean(&dlcache);
    if (NULL != dlinfo2.dli_sname) return false;

    // trust here is useless alignment data
    return true;
  }

  return false;
}
#endif

#ifdef SH_CONFIG_TRY_WITH_EXIT

// B T4: [-16M, +16M - 2]
#define SH_INST_T32_B_RANGE_LOW  (16777216)
#define SH_INST_T32_B_RANGE_HIGH (16777214)

static int sh_inst_hook_thumb_with_exit(sh_inst_t *self, uintptr_t target_addr, xdl_info_t *dlinfo,
                                        uintptr_t new_addr, uintptr_t *orig_addr, uintptr_t *orig_addr2) {
  int r;
  target_addr = SH_UTIL_CLEAR_BIT0(target_addr);
  uintptr_t pc = target_addr + 4;
  self->backup_len = 4;

#ifdef SH_CONFIG_DETECT_THUMB_TAIL_ALIGNED
  if (!sh_inst_thumb_is_long_enough(target_addr, self->backup_len, dlinfo))
    return SHADOWHOOK_ERRNO_HOOK_SYMSZ;
#else
  if (dlinfo->dli_ssize < self->backup_len) return SHADOWHOOK_ERRNO_HOOK_SYMSZ;
#endif

  // alloc an exit for absolute jump
  sh_t32_absolute_jump((uint16_t *)self->exit, true, new_addr);
  if (0 != (r = sh_exit_alloc(&self->exit_addr, &self->exit_type, pc, dlinfo, (uint8_t *)(self->exit),
                              sizeof(self->exit), SH_INST_T32_B_RANGE_LOW, SH_INST_T32_B_RANGE_HIGH)))
    return r;

  // rewrite
  if (0 != sh_util_mprotect(target_addr, dlinfo->dli_ssize, PROT_READ | PROT_WRITE | PROT_EXEC)) {
    r = SHADOWHOOK_ERRNO_MPROT;
    goto err;
  }
  size_t rewrite_len = 0;
  SH_SIG_TRY(SIGSEGV, SIGBUS) {
    r = sh_inst_hook_thumb_rewrite(self, target_addr, orig_addr, orig_addr2, &rewrite_len);
  }
  SH_SIG_CATCH() {
    r = SHADOWHOOK_ERRNO_HOOK_REWRITE_CRASH;
    goto err;
  }
  SH_SIG_EXIT
  if (0 != r) goto err;

  // relative jump to the exit by overwriting the head of original function
  sh_t32_relative_jump((uint16_t *)self->trampo, self->exit_addr, pc);
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
  if (0 != (r = sh_util_write_inst(target_addr, self->trampo, self->backup_len))) goto err;

  SH_LOG_INFO("thumb: hook (WITH EXIT) OK. target %" PRIxPTR " -> exit %" PRIxPTR " -> new %" PRIxPTR
              " -> enter %" PRIxPTR " -> remaining %" PRIxPTR,
              target_addr, self->exit_addr, new_addr, self->enter_addr,
              SH_UTIL_SET_BIT0(target_addr + rewrite_len));
  return 0;

err:
  sh_exit_free(self->exit_addr, self->exit_type, (uint8_t *)(self->exit), sizeof(self->exit));
  self->exit_addr = 0;  // this is a flag for with-exit or without-exit
  return r;
}
#endif

static int sh_inst_hook_thumb_without_exit(sh_inst_t *self, uintptr_t target_addr, xdl_info_t *dlinfo,
                                           uintptr_t new_addr, uintptr_t *orig_addr, uintptr_t *orig_addr2) {
  int r;
  target_addr = SH_UTIL_CLEAR_BIT0(target_addr);
  bool is_align4 = (0 == (target_addr % 4));
  self->backup_len = (is_align4 ? 8 : 10);

#ifdef SH_CONFIG_DETECT_THUMB_TAIL_ALIGNED
  if (!sh_inst_thumb_is_long_enough(target_addr, self->backup_len, dlinfo))
    return SHADOWHOOK_ERRNO_HOOK_SYMSZ;
#else
  if (dlinfo->dli_ssize < self->backup_len) return SHADOWHOOK_ERRNO_HOOK_SYMSZ;
#endif

  // rewrite
  if (0 != sh_util_mprotect(target_addr, dlinfo->dli_ssize, PROT_READ | PROT_WRITE | PROT_EXEC))
    return SHADOWHOOK_ERRNO_MPROT;
  size_t rewrite_len = 0;
  SH_SIG_TRY(SIGSEGV, SIGBUS) {
    r = sh_inst_hook_thumb_rewrite(self, target_addr, orig_addr, orig_addr2, &rewrite_len);
  }
  SH_SIG_CATCH() {
    return SHADOWHOOK_ERRNO_HOOK_REWRITE_CRASH;
  }
  SH_SIG_EXIT
  if (0 != r) return r;

  // absolute jump to new function address by overwriting the head of original function
  sh_t32_absolute_jump((uint16_t *)self->trampo, is_align4, new_addr);
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
  if (0 != (r = sh_util_write_inst(target_addr, self->trampo, self->backup_len))) return r;

  SH_LOG_INFO("thumb: hook (WITHOUT EXIT) OK. target %" PRIxPTR " -> new %" PRIxPTR " -> enter %" PRIxPTR
              " -> remaining %" PRIxPTR,
              target_addr, new_addr, self->enter_addr, SH_UTIL_SET_BIT0(target_addr + rewrite_len));
  return 0;
}

static int sh_inst_hook_arm_rewrite(sh_inst_t *self, uintptr_t target_addr, uintptr_t *orig_addr,
                                    uintptr_t *orig_addr2) {
  // backup original instructions (length: 4 or 8)
  memcpy((void *)(self->backup), (void *)target_addr, self->backup_len);

  // package the information passed to rewrite
  sh_a32_rewrite_info_t rinfo;
  rinfo.overwrite_start_addr = target_addr;
  rinfo.overwrite_end_addr = target_addr + self->backup_len;
  rinfo.rewrite_buf = (uint32_t *)self->enter_addr;
  rinfo.rewrite_buf_offset = 0;
  rinfo.rewrite_inst_lens_cnt = self->backup_len / 4;
  for (uintptr_t i = 0; i < self->backup_len; i += 4)
    rinfo.rewrite_inst_lens[i / 4] = sh_a32_get_rewrite_inst_len(*((uint32_t *)(target_addr + i)));

  // rewrite original instructions (fill in enter)
  uintptr_t pc = target_addr + 8;
  for (uintptr_t i = 0; i < self->backup_len; i += 4, pc += 4) {
    size_t offset = sh_a32_rewrite((uint32_t *)(self->enter_addr + rinfo.rewrite_buf_offset),
                                   *((uint32_t *)(target_addr + i)), pc, &rinfo);
    if (0 == offset) return SHADOWHOOK_ERRNO_HOOK_REWRITE_FAILED;
    rinfo.rewrite_buf_offset += offset;
  }

  // absolute jump back to remaining original instructions (fill in enter)
  rinfo.rewrite_buf_offset += sh_a32_absolute_jump((uint32_t *)(self->enter_addr + rinfo.rewrite_buf_offset),
                                                   target_addr + self->backup_len);
  sh_util_clear_cache(self->enter_addr, rinfo.rewrite_buf_offset);

  // save original function address
  if (NULL != orig_addr) __atomic_store_n(orig_addr, self->enter_addr, __ATOMIC_SEQ_CST);
  if (NULL != orig_addr2) __atomic_store_n(orig_addr2, self->enter_addr, __ATOMIC_SEQ_CST);
  return 0;
}

#ifdef SH_CONFIG_TRY_WITH_EXIT

// B A1: [-32M, +32M - 4]
#define SH_INST_A32_B_RANGE_LOW  (33554432)
#define SH_INST_A32_B_RANGE_HIGH (33554428)

static int sh_inst_hook_arm_with_exit(sh_inst_t *self, uintptr_t target_addr, xdl_info_t *dlinfo,
                                      uintptr_t new_addr, uintptr_t *orig_addr, uintptr_t *orig_addr2) {
  int r;
  uintptr_t pc = target_addr + 8;
  self->backup_len = 4;

  if (dlinfo->dli_ssize < self->backup_len) return SHADOWHOOK_ERRNO_HOOK_SYMSZ;

  // alloc an exit for absolute jump
  sh_a32_absolute_jump(self->exit, new_addr);
  if (0 != (r = sh_exit_alloc(&self->exit_addr, &self->exit_type, pc, dlinfo, (uint8_t *)(self->exit),
                              sizeof(self->exit), SH_INST_A32_B_RANGE_LOW, SH_INST_A32_B_RANGE_HIGH)))
    return r;

  // rewrite
  if (0 != sh_util_mprotect(target_addr, self->backup_len, PROT_READ | PROT_WRITE | PROT_EXEC)) {
    r = SHADOWHOOK_ERRNO_MPROT;
    goto err;
  }
  SH_SIG_TRY(SIGSEGV, SIGBUS) {
    r = sh_inst_hook_arm_rewrite(self, target_addr, orig_addr, orig_addr2);
  }
  SH_SIG_CATCH() {
    r = SHADOWHOOK_ERRNO_HOOK_REWRITE_CRASH;
    goto err;
  }
  SH_SIG_EXIT
  if (0 != r) goto err;

  // relative jump to the exit by overwriting the head of original function
  sh_a32_relative_jump(self->trampo, self->exit_addr, pc);
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
  if (0 != (r = sh_util_write_inst(target_addr, self->trampo, self->backup_len))) goto err;

  SH_LOG_INFO("a32: hook (WITH EXIT) OK. target %" PRIxPTR " -> exit %" PRIxPTR " -> new %" PRIxPTR
              " -> enter %" PRIxPTR " -> remaining %" PRIxPTR,
              target_addr, self->exit_addr, new_addr, self->enter_addr, target_addr + self->backup_len);
  return 0;

err:
  sh_exit_free(self->exit_addr, self->exit_type, (uint8_t *)(self->exit), sizeof(self->exit));
  self->exit_addr = 0;  // this is a flag for with-exit or without-exit
  return r;
}

#endif

static int sh_inst_hook_arm_without_exit(sh_inst_t *self, uintptr_t target_addr, xdl_info_t *dlinfo,
                                         uintptr_t new_addr, uintptr_t *orig_addr, uintptr_t *orig_addr2) {
  int r;
  self->backup_len = 8;

  if (dlinfo->dli_ssize < self->backup_len) return SHADOWHOOK_ERRNO_HOOK_SYMSZ;

  // rewrite
  if (0 != sh_util_mprotect(target_addr, self->backup_len, PROT_READ | PROT_WRITE | PROT_EXEC))
    return SHADOWHOOK_ERRNO_MPROT;
  SH_SIG_TRY(SIGSEGV, SIGBUS) {
    r = sh_inst_hook_arm_rewrite(self, target_addr, orig_addr, orig_addr2);
  }
  SH_SIG_CATCH() {
    return SHADOWHOOK_ERRNO_HOOK_REWRITE_CRASH;
  }
  SH_SIG_EXIT
  if (0 != r) return r;

  // absolute jump to new function address by overwriting the head of original function
  sh_a32_absolute_jump(self->trampo, new_addr);
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
  if (0 != (r = sh_util_write_inst(target_addr, self->trampo, self->backup_len))) return r;

  SH_LOG_INFO("a32: hook (WITHOUT EXIT) OK. target %" PRIxPTR " -> new %" PRIxPTR " -> enter %" PRIxPTR
              " -> remaining %" PRIxPTR,
              target_addr, new_addr, self->enter_addr, target_addr + self->backup_len);
  return 0;
}

int sh_inst_hook(sh_inst_t *self, uintptr_t target_addr, xdl_info_t *dlinfo, uintptr_t new_addr,
                 uintptr_t *orig_addr, uintptr_t *orig_addr2) {
  self->enter_addr = sh_enter_alloc();
  if (0 == self->enter_addr) return SHADOWHOOK_ERRNO_HOOK_ENTER;

  int r;
  if (SH_UTIL_IS_THUMB(target_addr)) {
#ifdef SH_CONFIG_TRY_WITH_EXIT
    if (0 == (r = sh_inst_hook_thumb_with_exit(self, target_addr, dlinfo, new_addr, orig_addr, orig_addr2)))
      return r;
#endif
    if (0 ==
        (r = sh_inst_hook_thumb_without_exit(self, target_addr, dlinfo, new_addr, orig_addr, orig_addr2)))
      return r;
  } else {
#ifdef SH_CONFIG_TRY_WITH_EXIT
    if (0 == (r = sh_inst_hook_arm_with_exit(self, target_addr, dlinfo, new_addr, orig_addr, orig_addr2)))
      return r;
#endif
    if (0 == (r = sh_inst_hook_arm_without_exit(self, target_addr, dlinfo, new_addr, orig_addr, orig_addr2)))
      return r;
  }

  // hook failed
  if (NULL != orig_addr) *orig_addr = 0;
  if (NULL != orig_addr2) *orig_addr2 = 0;
  sh_enter_free(self->enter_addr);
  return r;
}

int sh_inst_unhook(sh_inst_t *self, uintptr_t target_addr) {
  int r;
  bool is_thumb = SH_UTIL_IS_THUMB(target_addr);

  if (is_thumb) target_addr = SH_UTIL_CLEAR_BIT0(target_addr);

  // restore the instructions at the target address
  SH_SIG_TRY(SIGSEGV, SIGBUS) {
    r = memcmp((void *)target_addr, self->trampo, self->backup_len);
  }
  SH_SIG_CATCH() {
    return SHADOWHOOK_ERRNO_UNHOOK_CMP_CRASH;
  }
  SH_SIG_EXIT
  if (0 != r) return SHADOWHOOK_ERRNO_UNHOOK_TRAMPO_MISMATCH;
  if (0 != (r = sh_util_write_inst(target_addr, self->backup, self->backup_len))) return r;
  __atomic_thread_fence(__ATOMIC_SEQ_CST);

  // free memory space for exit
  if (0 != self->exit_addr)
    if (0 !=
        (r = sh_exit_free(self->exit_addr, self->exit_type, (uint8_t *)(self->exit), sizeof(self->exit))))
      return r;

  // free memory space for enter
  sh_enter_free(self->enter_addr);

  SH_LOG_INFO("%s: unhook OK. target %" PRIxPTR, is_thumb ? "thumb" : "a32", target_addr);
  return 0;
}
