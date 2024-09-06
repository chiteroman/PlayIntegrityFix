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

#include "bh_core.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "bh_cfi.h"
#include "bh_dl_monitor.h"
#include "bh_elf.h"
#include "bh_elf_manager.h"
#include "bh_linker.h"
#include "bh_log.h"
#include "bh_recorder.h"
#include "bh_task.h"
#include "bh_task_manager.h"
#include "bh_trampo.h"
#include "bh_util.h"
#include "bytehook.h"
#include "bytesig.h"

static bh_core_t bh_core = {.init_status = BYTEHOOK_STATUS_CODE_UNINIT,
                            .mode = -1,
                            .task_mgr = NULL,
                            .hook_mgr = NULL,
                            .elf_mgr = NULL};

bh_core_t *bh_core_global(void) {
  return &bh_core;
}

int bh_core_init(int mode, bool debug) {
  // Do not repeat the initialization.
  if (BYTEHOOK_STATUS_CODE_UNINIT != bh_core.init_status) {
    BH_LOG_SHOW("bytehook already inited, return: %d", bh_core.init_status);
    return bh_core.init_status;
  }

  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&lock);
  if (__predict_true(BYTEHOOK_STATUS_CODE_UNINIT == bh_core.init_status)) {
    int status;

    bh_log_set_debug(debug);
    if (BYTEHOOK_MODE_AUTOMATIC != mode && BYTEHOOK_MODE_MANUAL != mode) {
      status = BYTEHOOK_STATUS_CODE_INITERR_INVALID_ARG;
      goto end;
    }
    bh_core.mode = mode;
    if (0 != bh_linker_init()) {
      status = BYTEHOOK_STATUS_CODE_INITERR_SYM;
      goto end;
    }
    if (NULL == (bh_core.task_mgr = bh_task_manager_create())) {
      status = BYTEHOOK_STATUS_CODE_INITERR_TASK;
      goto end;
    }
    if (NULL == (bh_core.hook_mgr = bh_hook_manager_create())) {
      status = BYTEHOOK_STATUS_CODE_INITERR_HOOK;
      goto end;
    }
    if (NULL == (bh_core.elf_mgr = bh_elf_manager_create())) {
      status = BYTEHOOK_STATUS_CODE_INITERR_ELF;
      goto end;
    }
    if (BYTEHOOK_MODE_AUTOMATIC == mode && 0 != bh_trampo_init()) {
      status = BYTEHOOK_STATUS_CODE_INITERR_TRAMPO;
      goto end;
    }
    if (0 != bytesig_init(SIGSEGV) || 0 != bytesig_init(SIGBUS)) {
      status = BYTEHOOK_STATUS_CODE_INITERR_SIG;
      goto end;
    }
    if (0 != bh_cfi_disable_slowpath()) {
      status = BYTEHOOK_STATUS_CODE_INITERR_CFI;
      goto end;
    }
    status = BYTEHOOK_STATUS_CODE_OK;  // everything OK

  end:
    __atomic_store_n(&bh_core.init_status, status, __ATOMIC_SEQ_CST);
  }
  pthread_mutex_unlock(&lock);

  BH_LOG_SHOW("%s: bytehook init(mode: %s, debug: %s), return: %d", bytehook_get_version(),
              BYTEHOOK_MODE_AUTOMATIC == mode ? "AUTOMATIC" : "MANUAL", debug ? "true" : "false",
              bh_core.init_status);
  return bh_core.init_status;
}

bytehook_stub_t bh_core_hook_single(const char *caller_path_name, const char *callee_path_name,
                                    const char *sym_name, void *new_func, bytehook_hooked_t hooked,
                                    void *hooked_arg, uintptr_t caller_addr) {
  if (NULL == caller_path_name || NULL == sym_name || NULL == new_func) return NULL;
  if (BYTEHOOK_STATUS_CODE_OK != bh_core.init_status) return NULL;

  bh_task_t *task =
      bh_task_create_single(caller_path_name, callee_path_name, sym_name, new_func, hooked, hooked_arg);
  if (NULL != task) {
    bh_task_manager_add(bh_core.task_mgr, task);
    bh_task_manager_hook(bh_core.task_mgr, task);
    bh_recorder_add_hook(task->hook_status_code, caller_path_name, sym_name, (uintptr_t)new_func,
                         (uintptr_t)task, caller_addr);
  }
  return (bytehook_stub_t)task;
}

bytehook_stub_t bh_core_hook_partial(bytehook_caller_allow_filter_t caller_allow_filter,
                                     void *caller_allow_filter_arg, const char *callee_path_name,
                                     const char *sym_name, void *new_func, bytehook_hooked_t hooked,
                                     void *hooked_arg, uintptr_t caller_addr) {
  if (NULL == caller_allow_filter || NULL == sym_name || NULL == new_func) return NULL;
  if (BYTEHOOK_STATUS_CODE_OK != bh_core.init_status) return NULL;

  bh_task_t *task = bh_task_create_partial(caller_allow_filter, caller_allow_filter_arg, callee_path_name,
                                           sym_name, new_func, hooked, hooked_arg);
  if (NULL != task) {
    bh_task_manager_add(bh_core.task_mgr, task);
    bh_task_manager_hook(bh_core.task_mgr, task);
    bh_recorder_add_hook(BYTEHOOK_STATUS_CODE_MAX, "PARTIAL", sym_name, (uintptr_t)new_func, (uintptr_t)task,
                         caller_addr);
  }
  return (bytehook_stub_t)task;
}

bytehook_stub_t bh_core_hook_all(const char *callee_path_name, const char *sym_name, void *new_func,
                                 bytehook_hooked_t hooked, void *hooked_arg, uintptr_t caller_addr) {
  if (NULL == sym_name || NULL == new_func) return NULL;
  if (BYTEHOOK_STATUS_CODE_OK != bh_core.init_status) return NULL;

  bh_task_t *task = bh_task_create_all(callee_path_name, sym_name, new_func, hooked, hooked_arg);
  if (NULL != task) {
    bh_task_manager_add(bh_core.task_mgr, task);
    bh_task_manager_hook(bh_core.task_mgr, task);
    bh_recorder_add_hook(BYTEHOOK_STATUS_CODE_MAX, "ALL", sym_name, (uintptr_t)new_func, (uintptr_t)task,
                         caller_addr);
  }
  return (bytehook_stub_t)task;
}

int bh_core_unhook(bytehook_stub_t stub, uintptr_t caller_addr) {
  if (NULL == stub) return BYTEHOOK_STATUS_CODE_INVALID_ARG;
  if (BYTEHOOK_STATUS_CODE_OK != bh_core.init_status) return bh_core.init_status;

  bh_task_t *task = (bh_task_t *)stub;
  bh_task_manager_del(bh_core.task_mgr, task);
  int status_code = bh_task_manager_unhook(bh_core.task_mgr, task);
  bh_recorder_add_unhook(status_code, (uintptr_t)task, caller_addr);
  bh_task_destroy(&task);

  return status_code;
}

int bh_core_add_ignore(const char *caller_path_name) {
  int r = bh_elf_manager_add_ignore(bh_core.elf_mgr, caller_path_name);
  return 0 == r ? 0 : BYTEHOOK_STATUS_CODE_IGNORE;
}

bool bh_core_get_debug(void) {
  return bh_log_get_debug();
}

void bh_core_set_debug(bool debug) {
  bh_log_set_debug(debug);
}

bool bh_core_get_recordable(void) {
  return bh_recorder_get_recordable();
}

void bh_core_set_recordable(bool recordable) {
  bh_recorder_set_recordable(recordable);
}

void *bh_core_get_prev_func(void *func) {
  return bh_trampo_get_prev_func(func);
}

void bh_core_pop_stack(void *return_address) {
  bh_trampo_pop_stack(return_address);
}

void *bh_core_get_return_address(void) {
  return bh_trampo_get_return_address();
}

int bh_core_get_mode(void) {
  return bh_core.mode;
}

void bh_core_add_dlopen_callback(bytehook_pre_dlopen_t pre, bytehook_post_dlopen_t post, void *data) {
  bh_dl_monitor_add_dlopen_callback(pre, post, data);
}

void bh_core_del_dlopen_callback(bytehook_pre_dlopen_t pre, bytehook_post_dlopen_t post, void *data) {
  bh_dl_monitor_del_dlopen_callback(pre, post, data);
}
