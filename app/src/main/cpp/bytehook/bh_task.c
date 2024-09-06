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

#include "bh_task.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bh_core.h"
#include "bh_elf_manager.h"
#include "bh_hook_manager.h"
#include "bh_log.h"
#include "bh_recorder.h"
#include "bh_util.h"
#include "queue.h"

#define BH_TASK_ORIG_FUNC_UNSET   ((void *)0x0)
#define BH_TASK_ORIG_FUNC_INVALID ((void *)0x1)

static uint32_t bh_task_id_seed = 0;

static bh_task_t *bh_task_create(const char *callee_path_name, const char *sym_name, void *new_func,
                                 bytehook_hooked_t hooked, void *hooked_arg) {
  bh_task_t *self;
  if (NULL == (self = malloc(sizeof(bh_task_t)))) return NULL;
  self->id = __atomic_fetch_add(&bh_task_id_seed, 1, __ATOMIC_RELAXED);
  self->callee_path_name = (NULL != callee_path_name ? strdup(callee_path_name) : NULL);
  self->callee_addr = NULL;
  self->sym_name = strdup(sym_name);
  self->new_func = new_func;
  self->hooked = hooked;
  self->hooked_arg = hooked_arg;
  self->hook_status_code = BYTEHOOK_STATUS_CODE_MAX;
  self->manual_orig_func = BH_TASK_ORIG_FUNC_UNSET;

  return self;
}

bh_task_t *bh_task_create_single(const char *caller_path_name, const char *callee_path_name,
                                 const char *sym_name, void *new_func, bytehook_hooked_t hooked,
                                 void *hooked_arg) {
  bh_task_t *self;
  if (NULL == (self = bh_task_create(callee_path_name, sym_name, new_func, hooked, hooked_arg))) return NULL;
  self->type = BH_TASK_TYPE_SINGLE;
  self->status = BH_TASK_STATUS_UNFINISHED;
  self->caller_path_name = (NULL != caller_path_name ? strdup(caller_path_name) : NULL);
  return self;
}

bh_task_t *bh_task_create_partial(bytehook_caller_allow_filter_t caller_allow_filter,
                                  void *caller_allow_filter_arg, const char *callee_path_name,
                                  const char *sym_name, void *new_func, bytehook_hooked_t hooked,
                                  void *hooked_arg) {
  bh_task_t *self;
  if (NULL == (self = bh_task_create(callee_path_name, sym_name, new_func, hooked, hooked_arg))) return NULL;
  self->type = BH_TASK_TYPE_PARTIAL;
  self->status = BH_TASK_STATUS_LONGTERM;
  self->caller_path_name = NULL;
  self->caller_allow_filter = caller_allow_filter;
  self->caller_allow_filter_arg = caller_allow_filter_arg;
  return self;
}

bh_task_t *bh_task_create_all(const char *callee_path_name, const char *sym_name, void *new_func,
                              bytehook_hooked_t hooked, void *hooked_arg) {
  bh_task_t *self;
  if (NULL == (self = bh_task_create(callee_path_name, sym_name, new_func, hooked, hooked_arg))) return NULL;
  self->type = BH_TASK_TYPE_ALL;
  self->status = BH_TASK_STATUS_LONGTERM;
  self->caller_path_name = NULL;
  return self;
}

void bh_task_destroy(bh_task_t **self) {
  if (NULL == self || NULL == *self) return;

  if (NULL != (*self)->caller_path_name) free((*self)->caller_path_name);
  if (NULL != (*self)->callee_path_name) free((*self)->callee_path_name);
  if (NULL != (*self)->sym_name) free((*self)->sym_name);
  free(*self);
  *self = NULL;
}

static bool bh_task_hook_or_unhook(bh_task_t *self, bh_elf_t *elf) {
  void (*hook_or_unhook)(bh_hook_manager_t *, bh_task_t *, bh_elf_t *) =
      (BH_TASK_STATUS_UNHOOKING == self->status ? bh_hook_manager_unhook : bh_hook_manager_hook);

  switch (self->type) {
    case BH_TASK_TYPE_SINGLE:
      if (bh_elf_is_match(elf, self->caller_path_name)) {
        hook_or_unhook(bh_core_global()->hook_mgr, self, elf);
        if (BH_TASK_STATUS_UNHOOKING != self->status) self->status = BH_TASK_STATUS_FINISHED;
        return false;  // already found the ELF for single task, no need to continue
      }
      return true;  // continue
    case BH_TASK_TYPE_PARTIAL:
      if (self->caller_allow_filter(elf->pathname, self->caller_allow_filter_arg))
        hook_or_unhook(bh_core_global()->hook_mgr, self, elf);
      return true;  // continue
    case BH_TASK_TYPE_ALL:
      hook_or_unhook(bh_core_global()->hook_mgr, self, elf);
      return true;  // continue
  }
}

static bool bh_task_elf_iterate_cb(bh_elf_t *elf, void *arg) {
  return bh_task_hook_or_unhook((bh_task_t *)arg, elf);
}

static void bh_task_handle(bh_task_t *self) {
  switch (self->type) {
    case BH_TASK_TYPE_SINGLE: {
      bh_elf_t *caller_elf = bh_elf_manager_find_elf(bh_core_global()->elf_mgr, self->caller_path_name);
      if (NULL != caller_elf) bh_task_hook_or_unhook(self, caller_elf);
      break;
    }
    case BH_TASK_TYPE_ALL:
    case BH_TASK_TYPE_PARTIAL:
      bh_elf_manager_iterate(bh_core_global()->elf_mgr, bh_task_elf_iterate_cb, (void *)self);
      break;
  }
}

static int bh_task_check_pre_hook(bh_task_t *self) {
  // already finished, don't continue
  if (BH_TASK_STATUS_FINISHED == self->status) return -1;

  if (NULL != self->callee_path_name && NULL == self->callee_addr) {
    self->callee_addr =
        bh_elf_manager_find_export_addr(bh_core_global()->elf_mgr, self->callee_path_name, self->sym_name);

    // could not found callee by callee's pathname, don't continue
    if (NULL == self->callee_addr) return -1;
  }

  return 0;
}

void bh_task_hook(bh_task_t *self) {
  if (0 != bh_task_check_pre_hook(self)) return;

  bh_task_handle(self);
}

void bh_task_hook_elf(bh_task_t *self, bh_elf_t *elf) {
  if (0 != bh_task_check_pre_hook(self)) return;

  bh_task_hook_or_unhook(self, elf);
}

int bh_task_unhook(bh_task_t *self) {
  self->status = BH_TASK_STATUS_UNHOOKING;

  if (BYTEHOOK_MODE_MANUAL == bh_core_get_mode()) {
    if (BH_TASK_ORIG_FUNC_UNSET == self->manual_orig_func) {
      return BYTEHOOK_STATUS_CODE_OK;  // not hooked yet? not an error
    } else if (BH_TASK_ORIG_FUNC_INVALID == self->manual_orig_func) {
      BH_LOG_WARN("task: try to unhook with unmatch original function in manual mode");
      return BYTEHOOK_STATUS_CODE_UNMATCH_ORIG_FUNC;
    }
  }

  bh_task_handle(self);
  return BYTEHOOK_STATUS_CODE_OK;
}

void bh_task_set_manual_orig_func(bh_task_t *self, void *orig_func) {
  if (NULL == orig_func || BH_TASK_ORIG_FUNC_INVALID == orig_func) return;

  if (BH_TASK_ORIG_FUNC_UNSET == self->manual_orig_func) {
    self->manual_orig_func = orig_func;
  } else if (BH_TASK_ORIG_FUNC_INVALID == self->manual_orig_func) {
    return;
  } else {
    if (self->manual_orig_func != orig_func) self->manual_orig_func = BH_TASK_ORIG_FUNC_INVALID;
  }
}

void *bh_task_get_manual_orig_func(bh_task_t *self) {
  if (BH_TASK_ORIG_FUNC_UNSET == self->manual_orig_func) return NULL;
  if (BH_TASK_ORIG_FUNC_INVALID == self->manual_orig_func) return NULL;

  return self->manual_orig_func;
}

void bh_task_hooked(bh_task_t *self, int status_code, const char *caller_path_name, void *orig_func) {
  // single type task always with a caller_path_name
  if (BH_TASK_TYPE_SINGLE == self->type && NULL == caller_path_name)
    caller_path_name = self->caller_path_name;

  // save hook-status-code for single-task
  if (BYTEHOOK_STATUS_CODE_ORIG_ADDR != status_code && BH_TASK_TYPE_SINGLE == self->type &&
      BH_TASK_STATUS_UNHOOKING != self->status)
    self->hook_status_code = status_code;

  // do callback
  if (NULL != self->hooked && BH_TASK_STATUS_UNHOOKING != self->status)
    self->hooked(self, status_code, caller_path_name, self->sym_name, self->new_func, orig_func,
                 self->hooked_arg);
}
