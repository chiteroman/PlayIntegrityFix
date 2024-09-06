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
#include <stdint.h>

#include "bh_elf.h"
#include "bh_hook.h"
#include "bytehook.h"
#include "queue.h"

typedef enum { BH_TASK_TYPE_SINGLE = 0, BH_TASK_TYPE_ALL, BH_TASK_TYPE_PARTIAL } bh_task_type_t;

typedef enum {
  BH_TASK_STATUS_UNFINISHED = 0,
  BH_TASK_STATUS_FINISHED,
  BH_TASK_STATUS_LONGTERM,
  BH_TASK_STATUS_UNHOOKING
} bh_task_status_t;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct bh_task {
  uint32_t id;  // unique id
  bh_task_type_t type;
  bh_task_status_t status;

  // caller
  char *caller_path_name;                              // for single
  bytehook_caller_allow_filter_t caller_allow_filter;  // for partial
  void *caller_allow_filter_arg;                       // for partial

  // callee
  char *callee_path_name;
  void *callee_addr;

  // symbol
  char *sym_name;

  // new function address
  void *new_func;

  // callback
  bytehook_hooked_t hooked;
  void *hooked_arg;

  int hook_status_code;  // for single type

  void *manual_orig_func;  // for manual mode

  TAILQ_ENTRY(bh_task, ) link;
} bh_task_t;
#pragma clang diagnostic pop

bh_task_t *bh_task_create_single(const char *caller_path_name, const char *callee_path_name,
                                 const char *sym_name, void *new_func, bytehook_hooked_t hooked,
                                 void *hooked_arg);

bh_task_t *bh_task_create_partial(bytehook_caller_allow_filter_t caller_allow_filter,
                                  void *caller_allow_filter_arg, const char *callee_path_name,
                                  const char *sym_name, void *new_func, bytehook_hooked_t hooked,
                                  void *hooked_arg);

bh_task_t *bh_task_create_all(const char *callee_path_name, const char *sym_name, void *new_func,
                              bytehook_hooked_t hooked, void *hooked_arg);

void bh_task_destroy(bh_task_t **self);

void bh_task_hook(bh_task_t *self);
void bh_task_hook_elf(bh_task_t *self, bh_elf_t *elf);
int bh_task_unhook(bh_task_t *self);

void bh_task_set_manual_orig_func(bh_task_t *self, void *orig_func);
void *bh_task_get_manual_orig_func(bh_task_t *self);

void bh_task_hooked(bh_task_t *self, int status_code, const char *caller_path_name, void *orig_func);
