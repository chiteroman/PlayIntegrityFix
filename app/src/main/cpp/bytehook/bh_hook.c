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

#include "bh_hook.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bh_log.h"
#include "bh_task.h"

bh_hook_t *bh_hook_create(void *got_addr, void *orig_func) {
  bh_hook_t *self;

  if (NULL == (self = malloc(sizeof(bh_hook_t)))) return NULL;
  self->got_addr = got_addr;
  self->orig_func = orig_func;
  SLIST_INIT(&self->running_list);
  pthread_mutex_init(&self->running_list_lock, NULL);

  return self;
}

void bh_hook_destroy(bh_hook_t **self) {
  if (NULL == self || NULL == *self) return;

  pthread_mutex_destroy(&(*self)->running_list_lock);
  free(*self);
  *self = NULL;
}

int bh_hook_add_func(bh_hook_t *self, void *func, uint32_t task_id) {
  bh_hook_call_t *running;
  int r = BYTEHOOK_STATUS_CODE_OK;

  pthread_mutex_lock(&self->running_list_lock);

  // check repeated funcion
  SLIST_FOREACH(running, &self->running_list, link) {
    if (running->enabled && running->func == func) {
      r = BYTEHOOK_STATUS_CODE_REPEATED_FUNC;
      goto end;
    }
  }

  // try to re-enable an exists item
  SLIST_FOREACH(running, &self->running_list, link) {
    if (running->func == func && running->task_id == task_id) {
      if (!running->enabled) __atomic_store_n((bool *)&running->enabled, true, __ATOMIC_SEQ_CST);

      BH_LOG_INFO("hook chain: add(re-enable) func, GOT %" PRIxPTR ", func %" PRIxPTR,
                  (uintptr_t)self->got_addr, (uintptr_t)func);
      goto end;
    }
  }

  // create new item
  if (NULL == (running = malloc(sizeof(bh_hook_call_t)))) {
    r = BYTEHOOK_STATUS_CODE_APPEND_TRAMPO;
    goto end;
  }
  running->func = func;
  running->enabled = true;
  running->task_id = task_id;

  // insert to the head of the running_list
  //
  // equivalent to: SLIST_INSERT_HEAD(&self->running_list, running, link);
  // but: __ATOMIC_RELEASE ensures readers see only fully-constructed item
  SLIST_NEXT(running, link) = SLIST_FIRST(&self->running_list);
  __atomic_store_n((uintptr_t *)(&SLIST_FIRST(&self->running_list)), (uintptr_t)running, __ATOMIC_RELEASE);

  BH_LOG_INFO("hook chain: add(new) func, GOT %" PRIxPTR ", func %" PRIxPTR, (uintptr_t)self->got_addr,
              (uintptr_t)func);

end:
  pthread_mutex_unlock(&self->running_list_lock);
  return r;
}

bool bh_hook_del_func(bh_hook_t *self, void *func) {
  bool useful = false;

  pthread_mutex_lock(&self->running_list_lock);

  bh_hook_call_t *running;
  SLIST_FOREACH(running, &self->running_list, link) {
    if (running->func == func) {
      if (running->enabled) __atomic_store_n((bool *)&running->enabled, false, __ATOMIC_SEQ_CST);

      BH_LOG_INFO("hook chain: del func, GOT %" PRIxPTR ", func %" PRIxPTR, (uintptr_t)self->got_addr,
                  (uintptr_t)func);
    }

    if (running->enabled && !useful)
      useful = true;  // still useful, so do not remove the hook chain and the trampoline
  }

  pthread_mutex_unlock(&self->running_list_lock);

  return useful;
}
