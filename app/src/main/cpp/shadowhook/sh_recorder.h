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
#include <stdbool.h>
#include <stdint.h>

bool sh_recorder_get_recordable(void);
void sh_recorder_set_recordable(bool recordable);

int sh_recorder_add_hook(int error_number, bool is_hook_sym_addr, uintptr_t sym_addr, const char *lib_name,
                         const char *sym_name, uintptr_t new_addr, size_t backup_len, uintptr_t stub,
                         uintptr_t caller_addr);
int sh_recorder_add_unhook(int error_number, uintptr_t stub, uintptr_t caller_addr);

char *sh_recorder_get(uint32_t item_flags);
void sh_recorder_dump(int fd, uint32_t item_flags);
