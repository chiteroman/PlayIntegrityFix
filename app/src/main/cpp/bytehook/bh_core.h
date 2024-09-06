// Copyright (c) 2020-2022 ByteDance, Inc.
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

// Created by Kelun Cai (caikelun@bytedance.com) on 2020-06-02.

#pragma once
#include <stdbool.h>

#include "bh_elf_manager.h"
#include "bh_hook_manager.h"
#include "bh_task_manager.h"
#include "bytehook.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct {
  int init_status;
  int mode;
  bh_task_manager_t *task_mgr;
  bh_hook_manager_t *hook_mgr;
  bh_elf_manager_t *elf_mgr;
} bh_core_t;
#pragma clang diagnostic pop

bh_core_t *bh_core_global(void);

int bh_core_init(int mode, bool debug);

bytehook_stub_t bh_core_hook_single(const char *caller_path_name, const char *callee_path_name,
                                    const char *sym_name, void *new_func, bytehook_hooked_t hooked,
                                    void *hooked_arg, uintptr_t caller_addr);

bytehook_stub_t bh_core_hook_partial(bytehook_caller_allow_filter_t caller_allow_filter,
                                     void *caller_allow_filter_arg, const char *callee_path_name,
                                     const char *sym_name, void *new_func, bytehook_hooked_t hooked,
                                     void *hooked_arg, uintptr_t caller_addr);

bytehook_stub_t bh_core_hook_all(const char *callee_path_name, const char *sym_name, void *new_func,
                                 bytehook_hooked_t hooked, void *hooked_arg, uintptr_t caller_addr);

int bh_core_unhook(bytehook_stub_t stub, uintptr_t caller_addr);

int bh_core_add_ignore(const char *caller_path_name);

bool bh_core_get_debug(void);
void bh_core_set_debug(bool debug);
bool bh_core_get_recordable(void);
void bh_core_set_recordable(bool recordable);

void *bh_core_get_prev_func(void *func);

void *bh_core_get_return_address(void);

void bh_core_pop_stack(void *return_address);

int bh_core_get_mode(void);

void bh_core_add_dlopen_callback(bytehook_pre_dlopen_t pre, bytehook_post_dlopen_t post, void *data);

void bh_core_del_dlopen_callback(bytehook_pre_dlopen_t pre, bytehook_post_dlopen_t post, void *data);
