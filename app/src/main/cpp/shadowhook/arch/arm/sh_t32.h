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

#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sh_txx.h"

size_t sh_t32_get_rewrite_inst_len(uint16_t high_inst, uint16_t low_inst);
size_t sh_t32_rewrite(uint16_t *buf, uint16_t high_inst, uint16_t low_inst, uintptr_t pc,
                      sh_txx_rewrite_info_t *rinfo);

size_t sh_t32_absolute_jump(uint16_t *buf, bool is_align4, uintptr_t addr);
size_t sh_t32_relative_jump(uint16_t *buf, uintptr_t addr, uintptr_t pc);
