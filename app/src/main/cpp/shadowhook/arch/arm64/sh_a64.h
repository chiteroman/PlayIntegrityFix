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

#pragma once
#include <stddef.h>
#include <stdint.h>

#include "sh_inst.h"

typedef struct {
  uintptr_t start_addr;
  uintptr_t end_addr;
  uint32_t *buf;
  size_t buf_offset;
  size_t inst_lens[4];
  size_t inst_lens_cnt;
} sh_a64_rewrite_info_t;

size_t sh_a64_get_rewrite_inst_len(uint32_t inst);
size_t sh_a64_rewrite(uint32_t *buf, uint32_t inst, uintptr_t pc, sh_a64_rewrite_info_t *rinfo);

size_t sh_a64_absolute_jump_with_br(uint32_t *buf, uintptr_t addr);
size_t sh_a64_absolute_jump_with_ret(uint32_t *buf, uintptr_t addr);
size_t sh_a64_relative_jump(uint32_t *buf, uintptr_t addr, uintptr_t pc);
