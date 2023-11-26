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

#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sh_txx.h"

typedef struct {
  uint16_t insts[8];
  size_t insts_len;       // 2 - 16 (bytes)
  size_t insts_cnt;       // 1 - 4
  size_t insts_else_cnt;  // 0 - 3
  uintptr_t pcs[4];
  uint8_t firstcond;
  uint8_t padding[3];
} sh_t16_it_t;

bool sh_t16_parse_it(sh_t16_it_t *it, uint16_t inst, uintptr_t pc);
void sh_t16_rewrite_it_else(uint16_t *buf, uint16_t imm9, sh_t16_it_t *it);
void sh_t16_rewrite_it_then(uint16_t *buf, uint16_t imm12);

size_t sh_t16_get_rewrite_inst_len(uint16_t inst);
size_t sh_t16_rewrite(uint16_t *buf, uint16_t inst, uintptr_t pc, sh_txx_rewrite_info_t *rinfo);
