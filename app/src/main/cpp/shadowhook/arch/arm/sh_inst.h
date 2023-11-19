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
#include <stdint.h>

#include "xdl.h"

typedef struct {
  uint32_t trampo[4];   // align 16 // length == backup_len
  uint8_t backup[16];   // align 16
  uint16_t backup_len;  // == 4 or 8 or 10
  uint16_t exit_type;
  uintptr_t exit_addr;
  uint32_t exit[2];
  uintptr_t enter_addr;
} sh_inst_t;

int sh_inst_hook(sh_inst_t *self, uintptr_t target_addr, xdl_info_t *dlinfo, uintptr_t new_addr,
                 uintptr_t *orig_addr, uintptr_t *orig_addr2);
int sh_inst_unhook(sh_inst_t *self, uintptr_t target_addr);
