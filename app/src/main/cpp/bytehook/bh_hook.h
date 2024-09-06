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
#include <pthread.h>
#include <stdbool.h>

#include "queue.h"
#include "tree.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"

typedef struct bh_hook_call {
  void *func;
  bool enabled;
  uint32_t task_id;
  SLIST_ENTRY(bh_hook_call, ) link;
} bh_hook_call_t;
typedef SLIST_HEAD(bh_hook_call_list, bh_hook_call, ) bh_hook_call_list_t;

typedef struct bh_hook {
  void *got_addr;
  void *orig_func;
  bh_hook_call_list_t running_list;
  pthread_mutex_t running_list_lock;
  RB_ENTRY(bh_hook) link;
} bh_hook_t;

#pragma clang diagnostic pop

bh_hook_t *bh_hook_create(void *got_addr, void *orig_func);
void bh_hook_destroy(bh_hook_t **self);

int bh_hook_add_func(bh_hook_t *self, void *func, uint32_t task_id);
bool bh_hook_del_func(bh_hook_t *self, void *func);
