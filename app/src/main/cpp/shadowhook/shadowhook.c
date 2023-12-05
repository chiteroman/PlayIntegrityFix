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

#include "shadowhook.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

#include "bytesig.h"
#include "sh_enter.h"
#include "sh_errno.h"
#include "sh_exit.h"
#include "sh_hub.h"
#include "sh_linker.h"
#include "sh_log.h"
#include "sh_recorder.h"
#include "sh_safe.h"
#include "sh_sig.h"
#include "sh_switch.h"
#include "sh_task.h"
#include "sh_util.h"
#include "xdl.h"

#define GOTO_ERR(errnum) \
  do {                   \
    r = errnum;          \
    goto err;            \
  } while (0)

static int shadowhook_init_errno = SHADOWHOOK_ERRNO_UNINIT;
static shadowhook_mode_t shadowhook_mode = SHADOWHOOK_MODE_SHARED;

const char *shadowhook_get_version(void) {
  return "shadowhook version " SHADOWHOOK_VERSION;
}

int shadowhook_init(shadowhook_mode_t mode, bool debuggable) {
  bool do_init = false;

  if (__predict_true(SHADOWHOOK_ERRNO_UNINIT == shadowhook_init_errno)) {
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&lock);
    if (__predict_true(SHADOWHOOK_ERRNO_UNINIT == shadowhook_init_errno)) {
      do_init = true;
      shadowhook_mode = mode;
      sh_log_set_debuggable(debuggable);

#define GOTO_END(errnum)            \
  do {                              \
    shadowhook_init_errno = errnum; \
    goto end;                       \
  } while (0)

      if (__predict_false(0 != sh_errno_init())) GOTO_END(SHADOWHOOK_ERRNO_INIT_ERRNO);
      if (__predict_false(0 != bytesig_init(SIGSEGV))) GOTO_END(SHADOWHOOK_ERRNO_INIT_SIGSEGV);
      if (__predict_false(0 != bytesig_init(SIGBUS))) GOTO_END(SHADOWHOOK_ERRNO_INIT_SIGBUS);
      if (__predict_false(0 != sh_enter_init())) GOTO_END(SHADOWHOOK_ERRNO_INIT_ENTER);
      sh_exit_init();
      if (SHADOWHOOK_MODE_SHARED == shadowhook_mode) {
        if (__predict_false(0 != sh_safe_init())) GOTO_END(SHADOWHOOK_ERRNO_INIT_SAFE);
        if (__predict_false(0 != sh_hub_init())) GOTO_END(SHADOWHOOK_ERRNO_INIT_HUB);
      } else {
        if (__predict_false(0 != sh_linker_init())) GOTO_END(SHADOWHOOK_ERRNO_INIT_LINKER);
      }

#undef GOTO_END

      shadowhook_init_errno = SHADOWHOOK_ERRNO_OK;
    }
  end:
    pthread_mutex_unlock(&lock);
  }

  SH_LOG_ALWAYS_SHOW("%s: shadowhook init(mode: %s, debuggable: %s), return: %d, real-init: %s",
                     shadowhook_get_version(), SHADOWHOOK_MODE_SHARED == mode ? "SHARED" : "UNIQUE",
                     debuggable ? "true" : "false", shadowhook_init_errno, do_init ? "yes" : "no");
  SH_ERRNO_SET_RET_ERRNUM(shadowhook_init_errno);
}

int shadowhook_get_init_errno(void) {
  return shadowhook_init_errno;
}

shadowhook_mode_t shadowhook_get_mode(void) {
  return shadowhook_mode;
}

bool shadowhook_get_debuggable(void) {
  return sh_log_get_debuggable();
}

void shadowhook_set_debuggable(bool debuggable) {
  sh_log_set_debuggable(debuggable);
}

bool shadowhook_get_recordable(void) {
  return sh_recorder_get_recordable();
}

void shadowhook_set_recordable(bool recordable) {
  sh_recorder_set_recordable(recordable);
}

int shadowhook_get_errno(void) {
  return sh_errno_get();
}

const char *shadowhook_to_errmsg(int error_number) {
  return sh_errno_to_errmsg(error_number);
}

static void *shadowhook_hook_addr_impl(void *sym_addr, void *new_addr, void **orig_addr,
                                       bool ignore_symbol_check, uintptr_t caller_addr) {
  SH_LOG_INFO("shadowhook: hook_%s_addr(%p, %p) ...", ignore_symbol_check ? "func" : "sym", sym_addr,
              new_addr);
  sh_errno_reset();

  int r;
  if (NULL == sym_addr || NULL == new_addr) GOTO_ERR(SHADOWHOOK_ERRNO_INVALID_ARG);
  if (SHADOWHOOK_ERRNO_OK != shadowhook_init_errno) GOTO_ERR(shadowhook_init_errno);

  // create task
  sh_task_t *task =
      sh_task_create_by_target_addr((uintptr_t)sym_addr, (uintptr_t)new_addr, (uintptr_t *)orig_addr,
                                    ignore_symbol_check, (uintptr_t)caller_addr);
  if (NULL == task) GOTO_ERR(SHADOWHOOK_ERRNO_OOM);

  // do hook
  r = sh_task_hook(task);
  if (0 != r) {
    sh_task_destroy(task);
    GOTO_ERR(r);
  }

  // OK
  SH_LOG_INFO("shadowhook: hook_%s_addr(%p, %p) OK. return: %p", ignore_symbol_check ? "func" : "sym",
              sym_addr, new_addr, (void *)task);
  SH_ERRNO_SET_RET(SHADOWHOOK_ERRNO_OK, (void *)task);

err:
  SH_LOG_ERROR("shadowhook: hook_%s_addr(%p, %p) FAILED. %d - %s", ignore_symbol_check ? "func" : "sym",
               sym_addr, new_addr, r, sh_errno_to_errmsg(r));
  SH_ERRNO_SET_RET_NULL(r);
}

void *shadowhook_hook_func_addr(void *func_addr, void *new_addr, void **orig_addr) {
  const void *caller_addr = __builtin_return_address(0);
  return shadowhook_hook_addr_impl(func_addr, new_addr, orig_addr, true, (uintptr_t)caller_addr);
}

void *shadowhook_hook_sym_addr(void *sym_addr, void *new_addr, void **orig_addr) {
  const void *caller_addr = __builtin_return_address(0);
  return shadowhook_hook_addr_impl(sym_addr, new_addr, orig_addr, false, (uintptr_t)caller_addr);
}

static void *shadowhook_hook_sym_name_impl(const char *lib_name, const char *sym_name, void *new_addr,
                                           void **orig_addr, shadowhook_hooked_t hooked, void *hooked_arg,
                                           uintptr_t caller_addr) {
  SH_LOG_INFO("shadowhook: hook_sym_name(%s, %s, %p) ...", lib_name, sym_name, new_addr);
  sh_errno_reset();

  int r;
  if (NULL == lib_name || NULL == sym_name || NULL == new_addr) GOTO_ERR(SHADOWHOOK_ERRNO_INVALID_ARG);
  if (SHADOWHOOK_ERRNO_OK != shadowhook_init_errno) GOTO_ERR(shadowhook_init_errno);

  // create task
  sh_task_t *task =
      sh_task_create_by_sym_name(lib_name, sym_name, (uintptr_t)new_addr, (uintptr_t *)orig_addr, hooked,
                                 hooked_arg, (uintptr_t)caller_addr);
  if (NULL == task) GOTO_ERR(SHADOWHOOK_ERRNO_OOM);

  // do hook
  r = sh_task_hook(task);
  if (0 != r && SHADOWHOOK_ERRNO_PENDING != r) {
    sh_task_destroy(task);
    GOTO_ERR(r);
  }

  // OK
  SH_LOG_INFO("shadowhook: hook_sym_name(%s, %s, %p) OK. return: %p. %d - %s", lib_name, sym_name, new_addr,
              (void *)task, r, sh_errno_to_errmsg(r));
  SH_ERRNO_SET_RET(r, (void *)task);

err:
  SH_LOG_ERROR("shadowhook: hook_sym_name(%s, %s, %p) FAILED. %d - %s", lib_name, sym_name, new_addr, r,
               sh_errno_to_errmsg(r));
  SH_ERRNO_SET_RET_NULL(r);
}

void *shadowhook_hook_sym_name(const char *lib_name, const char *sym_name, void *new_addr, void **orig_addr) {
  const void *caller_addr = __builtin_return_address(0);
  return shadowhook_hook_sym_name_impl(lib_name, sym_name, new_addr, orig_addr, NULL, NULL,
                                       (uintptr_t)caller_addr);
}

void *shadowhook_hook_sym_name_callback(const char *lib_name, const char *sym_name, void *new_addr,
                                        void **orig_addr, shadowhook_hooked_t hooked, void *hooked_arg) {
  const void *caller_addr = __builtin_return_address(0);
  return shadowhook_hook_sym_name_impl(lib_name, sym_name, new_addr, orig_addr, hooked, hooked_arg,
                                       (uintptr_t)caller_addr);
}

int shadowhook_unhook(void *stub) {
  const void *caller_addr = __builtin_return_address(0);
  SH_LOG_INFO("shadowhook: unhook(%p) ...", stub);
  sh_errno_reset();

  int r;
  if (NULL == stub) GOTO_ERR(SHADOWHOOK_ERRNO_INVALID_ARG);
  if (SHADOWHOOK_ERRNO_OK != shadowhook_init_errno) GOTO_ERR(shadowhook_init_errno);

  sh_task_t *task = (sh_task_t *)stub;
  r = sh_task_unhook(task, (uintptr_t)caller_addr);
  sh_task_destroy(task);
  if (0 != r) GOTO_ERR(r);

  // OK
  SH_LOG_INFO("shadowhook: unhook(%p) OK", stub);
  SH_ERRNO_SET_RET_ERRNUM(SHADOWHOOK_ERRNO_OK);

err:
  SH_LOG_ERROR("shadowhook: unhook(%p) FAILED. %d - %s", stub, r, sh_errno_to_errmsg(r));
  SH_ERRNO_SET_RET_FAIL(r);
}

char *shadowhook_get_records(uint32_t item_flags) {
  return sh_recorder_get(item_flags);
}

void shadowhook_dump_records(int fd, uint32_t item_flags) {
  sh_recorder_dump(fd, item_flags);
}

void *shadowhook_dlopen(const char *lib_name) {
  void *handle = NULL;
  if (sh_util_get_api_level() >= __ANDROID_API_L__) {
    handle = xdl_open(lib_name, XDL_DEFAULT);
  } else {
    SH_SIG_TRY(SIGSEGV, SIGBUS) {
      handle = xdl_open(lib_name, XDL_DEFAULT);
    }
    SH_SIG_CATCH() {
      SH_LOG_WARN("shadowhook: dlopen crashed - %s", lib_name);
    }
    SH_SIG_EXIT
  }
  return handle;
}

void shadowhook_dlclose(void *handle) {
  xdl_close(handle);
}

void *shadowhook_dlsym(void *handle, const char *sym_name) {
  void *addr = shadowhook_dlsym_dynsym(handle, sym_name);
  if (NULL == addr) addr = shadowhook_dlsym_symtab(handle, sym_name);
  return addr;
}

void *shadowhook_dlsym_dynsym(void *handle, const char *sym_name) {
  void *addr = NULL;
  SH_SIG_TRY(SIGSEGV, SIGBUS) {
    addr = xdl_sym(handle, sym_name, NULL);
  }
  SH_SIG_CATCH() {
    SH_LOG_WARN("shadowhook: dlsym_dynsym crashed - %p, %s", handle, sym_name);
  }
  SH_SIG_EXIT
  return addr;
}

void *shadowhook_dlsym_symtab(void *handle, const char *sym_name) {
  void *addr = NULL;
  SH_SIG_TRY(SIGSEGV, SIGBUS) {
    addr = xdl_dsym(handle, sym_name, NULL);
  }
  SH_SIG_CATCH() {
    SH_LOG_WARN("shadowhook: dlsym_symtab crashed - %p, %s", handle, sym_name);
  }
  SH_SIG_EXIT
  return addr;
}

void *shadowhook_get_prev_func(void *func) {
  if (__predict_false(SHADOWHOOK_IS_UNIQUE_MODE)) abort();
  return sh_hub_get_prev_func(func);
}

void shadowhook_pop_stack(void *return_address) {
  if (__predict_false(SHADOWHOOK_IS_UNIQUE_MODE)) abort();
  sh_hub_pop_stack(return_address);
}

void shadowhook_allow_reentrant(void *return_address) {
  if (__predict_false(SHADOWHOOK_IS_UNIQUE_MODE)) abort();
  sh_hub_allow_reentrant(return_address);
}

void shadowhook_disallow_reentrant(void *return_address) {
  if (__predict_false(SHADOWHOOK_IS_UNIQUE_MODE)) abort();
  sh_hub_disallow_reentrant(return_address);
}

void *shadowhook_get_return_address(void) {
  if (__predict_false(SHADOWHOOK_IS_UNIQUE_MODE)) abort();
  return sh_hub_get_return_address();
}
