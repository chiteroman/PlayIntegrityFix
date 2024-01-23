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

#include "sh_txx.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "sh_log.h"
#include "sh_util.h"

bool sh_txx_is_addr_need_fix(uintptr_t addr, sh_txx_rewrite_info_t *rinfo) {
  return (rinfo->start_addr <= addr && addr < rinfo->end_addr);
}

uintptr_t sh_txx_fix_addr(uintptr_t addr, sh_txx_rewrite_info_t *rinfo) {
  bool is_thumb = SH_UTIL_IS_THUMB(addr);

  if (is_thumb) addr = SH_UTIL_CLEAR_BIT0(addr);

  if (rinfo->start_addr <= addr && addr < rinfo->end_addr) {
    uintptr_t cursor_addr = rinfo->start_addr;
    size_t offset = 0;
    for (size_t i = 0; i < rinfo->inst_lens_cnt; i++) {
      if (cursor_addr >= addr) break;
      cursor_addr += 2;
      offset += rinfo->inst_lens[i];
    }
    uintptr_t fixed_addr = (uintptr_t)rinfo->buf + offset;
    if (is_thumb) fixed_addr = SH_UTIL_SET_BIT0(fixed_addr);

    SH_LOG_INFO("txx rewrite: fix addr %" PRIxPTR " -> %" PRIxPTR, addr, fixed_addr);
    return fixed_addr;
  }

  if (is_thumb) addr = SH_UTIL_SET_BIT0(addr);
  return addr;
}
