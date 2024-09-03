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

#include "sh_inst.h"

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "sh_a64.h"
#include "sh_config.h"
#include "sh_enter.h"
#include "sh_exit.h"
#include "sh_log.h"
#include "sh_sig.h"
#include "sh_util.h"
#include "shadowhook.h"
#include "xdl.h"

static int sh_inst_hook_rewrite(sh_inst_t *self, uintptr_t target_addr, uintptr_t *orig_addr,
                                uintptr_t *orig_addr2) {
  // backup original instructions (length: 4 or 16)
  memcpy((void *)(self->backup), (void *)target_addr, self->backup_len);

  // package the information passed to rewrite
  sh_a64_rewrite_info_t rinfo;
  rinfo.start_addr = target_addr;
  rinfo.end_addr = target_addr + self->backup_len;
  rinfo.buf = (uint32_t *)self->enter_addr;
  rinfo.buf_offset = 0;
  rinfo.inst_lens_cnt = self->backup_len / 4;
  for (uintptr_t i = 0; i < self->backup_len; i += 4)
    rinfo.inst_lens[i / 4] = sh_a64_get_rewrite_inst_len(*((uint32_t *)(target_addr + i)));

  // rewrite original instructions (fill in enter)
  uintptr_t pc = target_addr;
  for (uintptr_t i = 0; i < self->backup_len; i += 4, pc += 4) {
    size_t offset = sh_a64_rewrite((uint32_t *)(self->enter_addr + rinfo.buf_offset),
                                   *((uint32_t *)(target_addr + i)), pc, &rinfo);
    if (0 == offset) return SHADOWHOOK_ERRNO_HOOK_REWRITE_FAILED;
    rinfo.buf_offset += offset;
  }

  // absolute jump back to remaining original instructions (fill in enter)
  rinfo.buf_offset += sh_a64_absolute_jump_with_ret((uint32_t *)(self->enter_addr + rinfo.buf_offset),
                                                    target_addr + self->backup_len);
  sh_util_clear_cache(self->enter_addr, rinfo.buf_offset);

  // save original function address
  if (NULL != orig_addr) __atomic_store_n(orig_addr, self->enter_addr, __ATOMIC_SEQ_CST);
  if (NULL != orig_addr2) __atomic_store_n(orig_addr2, self->enter_addr, __ATOMIC_SEQ_CST);
  return 0;
}

#ifdef SH_CONFIG_TRY_WITH_EXIT

// B: [-128M, +128M - 4]
#define SH_INST_A64_B_RANGE_LOW  (134217728)
#define SH_INST_A64_B_RANGE_HIGH (134217724)

static int sh_inst_hook_with_exit(sh_inst_t *self, uintptr_t target_addr, xdl_info_t *dlinfo,
                                  uintptr_t new_addr, uintptr_t *orig_addr, uintptr_t *orig_addr2) {
  int r;
  uintptr_t pc = target_addr;
  self->backup_len = 4;

  if (dlinfo->dli_ssize < self->backup_len) return SHADOWHOOK_ERRNO_HOOK_SYMSZ;

  // alloc an exit for absolute jump
  sh_a64_absolute_jump_with_br(self->exit, new_addr);
  if (0 !=
      (r = sh_exit_alloc(&self->exit_addr, (uint16_t *)&self->exit_type, pc, dlinfo, (uint8_t *)(self->exit),
                         sizeof(self->exit), SH_INST_A64_B_RANGE_LOW, SH_INST_A64_B_RANGE_HIGH)))
    return r;

  // rewrite
  if (0 != sh_util_mprotect(target_addr, self->backup_len, PROT_READ | PROT_WRITE | PROT_EXEC)) {
    r = SHADOWHOOK_ERRNO_MPROT;
    goto err;
  }
  SH_SIG_TRY(SIGSEGV, SIGBUS) {
    r = sh_inst_hook_rewrite(self, target_addr, orig_addr, orig_addr2);
  }
  SH_SIG_CATCH() {
    r = SHADOWHOOK_ERRNO_HOOK_REWRITE_CRASH;
    goto err;
  }
  SH_SIG_EXIT
  if (0 != r) goto err;

  // relative jump to the exit by overwriting the head of original function
  sh_a64_relative_jump(self->trampo, self->exit_addr, pc);
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
  if (0 != (r = sh_util_write_inst(target_addr, self->trampo, self->backup_len))) goto err;

  SH_LOG_INFO("a64: hook (WITH EXIT) OK. target %" PRIxPTR " -> exit %" PRIxPTR " -> new %" PRIxPTR
              " -> enter %" PRIxPTR " -> remaining %" PRIxPTR,
              target_addr, self->exit_addr, new_addr, self->enter_addr, target_addr + self->backup_len);
  return 0;

err:
  sh_exit_free(self->exit_addr, (uint16_t)self->exit_type, (uint8_t *)(self->exit), sizeof(self->exit));
  self->exit_addr = 0;  // this is a flag for with-exit or without-exit
  return r;
}
#endif

static int sh_inst_hook_without_exit(sh_inst_t *self, uintptr_t target_addr, xdl_info_t *dlinfo,
                                     uintptr_t new_addr, uintptr_t *orig_addr, uintptr_t *orig_addr2) {
  int r;
  self->backup_len = 16;

  if (dlinfo->dli_ssize < self->backup_len) return SHADOWHOOK_ERRNO_HOOK_SYMSZ;

  // rewrite
  if (0 != sh_util_mprotect(target_addr, self->backup_len, PROT_READ | PROT_WRITE | PROT_EXEC))
    return SHADOWHOOK_ERRNO_MPROT;
  SH_SIG_TRY(SIGSEGV, SIGBUS) {
    r = sh_inst_hook_rewrite(self, target_addr, orig_addr, orig_addr2);
  }
  SH_SIG_CATCH() {
    return SHADOWHOOK_ERRNO_HOOK_REWRITE_CRASH;
  }
  SH_SIG_EXIT
  if (0 != r) return r;

  // absolute jump to new function address by overwriting the head of original function
  sh_a64_absolute_jump_with_br(self->trampo, new_addr);
  __atomic_thread_fence(__ATOMIC_SEQ_CST);
  if (0 != (r = sh_util_write_inst(target_addr, self->trampo, self->backup_len))) return r;

  SH_LOG_INFO("a64: hook (WITHOUT EXIT) OK. target %" PRIxPTR " -> new %" PRIxPTR " -> enter %" PRIxPTR
              " -> remaining %" PRIxPTR,
              target_addr, new_addr, self->enter_addr, target_addr + self->backup_len);
  return 0;
}

int sh_inst_hook(sh_inst_t *self, uintptr_t target_addr, xdl_info_t *dlinfo, uintptr_t new_addr,
                 uintptr_t *orig_addr, uintptr_t *orig_addr2) {
  self->enter_addr = sh_enter_alloc();
  if (0 == self->enter_addr) return SHADOWHOOK_ERRNO_HOOK_ENTER;

  int r;
#ifdef SH_CONFIG_TRY_WITH_EXIT
  if (0 == (r = sh_inst_hook_with_exit(self, target_addr, dlinfo, new_addr, orig_addr, orig_addr2))) return r;
#endif
  if (0 == (r = sh_inst_hook_without_exit(self, target_addr, dlinfo, new_addr, orig_addr, orig_addr2)))
    return r;

  // hook failed
  if (NULL != orig_addr) *orig_addr = 0;
  if (NULL != orig_addr2) *orig_addr2 = 0;
  sh_enter_free(self->enter_addr);
  return r;
}

int sh_inst_unhook(sh_inst_t *self, uintptr_t target_addr) {
  int r;

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
    if (0 != (r = sh_exit_free(self->exit_addr, (uint16_t)self->exit_type, (uint8_t *)(self->exit),
                               sizeof(self->exit))))
      return r;

  // free memory space for enter
  sh_enter_free(self->enter_addr);

  SH_LOG_INFO("a64: unhook OK. target %" PRIxPTR, target_addr);
  return 0;
}
