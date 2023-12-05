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
#include <stdint.h>

#include "xdl.h"

int sh_linker_init(void);

const char *sh_linker_match_dlfcn(uintptr_t target_addr);
bool sh_linker_need_to_hook_dlopen(uintptr_t target_addr);

typedef void (*sh_linker_post_dlopen_t)(void *arg);
int sh_linker_hook_dlopen(sh_linker_post_dlopen_t post_dlopen, void *post_dlopen_arg);

int sh_linker_get_dlinfo_by_addr(void *addr, xdl_info_t *dlinfo, char *lib_name, size_t lib_name_sz,
                                 char *sym_name, size_t sym_name_sz, bool ignore_symbol_check);
int sh_linker_get_dlinfo_by_sym_name(const char *lib_name, const char *sym_name, xdl_info_t *dlinfo,
                                     char *real_lib_name, size_t real_lib_name_sz);
