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

#include "bytehook.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "bh_core.h"
#include "bh_recorder.h"

const char *bytehook_get_version(void) {
  return "bytehook version " BYTEHOOK_VERSION;
}

int bytehook_init(int mode, bool debug) {
  return bh_core_init(mode, debug);
}

bytehook_stub_t bytehook_hook_single(const char *caller_path_name, const char *callee_path_name,
                                     const char *sym_name, void *new_func, bytehook_hooked_t hooked,
                                     void *hooked_arg) {
  const void *caller_addr = __builtin_return_address(0);
  return bh_core_hook_single(caller_path_name, callee_path_name, sym_name, new_func, hooked, hooked_arg,
                             (uintptr_t)caller_addr);
}

bytehook_stub_t bytehook_hook_partial(bytehook_caller_allow_filter_t caller_allow_filter,
                                      void *caller_allow_filter_arg, const char *callee_path_name,
                                      const char *sym_name, void *new_func, bytehook_hooked_t hooked,
                                      void *hooked_arg) {
  const void *caller_addr = __builtin_return_address(0);
  return bh_core_hook_partial(caller_allow_filter, caller_allow_filter_arg, callee_path_name, sym_name,
                              new_func, hooked, hooked_arg, (uintptr_t)caller_addr);
}

bytehook_stub_t bytehook_hook_all(const char *callee_path_name, const char *sym_name, void *new_func,
                                  bytehook_hooked_t hooked, void *hooked_arg) {
  const void *caller_addr = __builtin_return_address(0);
  return bh_core_hook_all(callee_path_name, sym_name, new_func, hooked, hooked_arg, (uintptr_t)caller_addr);
}

int bytehook_unhook(bytehook_stub_t stub) {
  const void *caller_addr = __builtin_return_address(0);
  return bh_core_unhook(stub, (uintptr_t)caller_addr);
}

int bytehook_add_ignore(const char *caller_path_name) {
  return bh_core_add_ignore(caller_path_name);
}

bool bytehook_get_debug(void) {
  return bh_core_get_debug();
}

void bytehook_set_debug(bool debug) {
  bh_core_set_debug(debug);
}

bool bytehook_get_recordable(void) {
  return bh_core_get_recordable();
}

void bytehook_set_recordable(bool recordable) {
  bh_core_set_recordable(recordable);
}

char *bytehook_get_records(uint32_t item_flags) {
  return bh_recorder_get(item_flags);
}

void bytehook_dump_records(int fd, uint32_t item_flags) {
  bh_recorder_dump(fd, item_flags);
}

void *bytehook_get_prev_func(void *func) {
  if (__predict_false(BYTEHOOK_MODE_MANUAL == bh_core_get_mode())) abort();
  return bh_core_get_prev_func(func);
}

void *bytehook_get_return_address(void) {
  if (__predict_false(BYTEHOOK_MODE_MANUAL == bh_core_get_mode())) abort();
  return bh_core_get_return_address();
}

void bytehook_pop_stack(void *return_address) {
  if (__predict_false(BYTEHOOK_MODE_MANUAL == bh_core_get_mode())) abort();
  bh_core_pop_stack(return_address);
}

int bytehook_get_mode(void) {
  return bh_core_get_mode();
}

void bytehook_add_dlopen_callback(bytehook_pre_dlopen_t pre, bytehook_post_dlopen_t post, void *data) {
  bh_core_add_dlopen_callback(pre, post, data);
}

void bytehook_del_dlopen_callback(bytehook_pre_dlopen_t pre, bytehook_post_dlopen_t post, void *data) {
  bh_core_del_dlopen_callback(pre, post, data);
}
