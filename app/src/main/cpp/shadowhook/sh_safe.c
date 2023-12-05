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

#include "sh_safe.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "sh_util.h"
#include "xdl.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#pragma clang diagnostic ignored "-Wvariadic-macros"
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wpacked"
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#include "linux_syscall_support.h"
#pragma clang diagnostic pop

#define SH_SAFE_IDX_PTHREAD_GETSPECIFIC 0
#define SH_SAFE_IDX_PTHREAD_SETSPECIFIC 1
#define SH_SAFE_IDX_ABORT               2

#define SH_SAFE_IDX_SZ 3
typedef struct {
  uintptr_t target_addr;
  uintptr_t orig_addr;
} sh_safe_addr_t;
static sh_safe_addr_t sh_safe_addrs[SH_SAFE_IDX_SZ];
static int sh_safe_api_level;

static int sh_safe_init_func(void *handle, const char *symbol, size_t idx) {
  sh_safe_addrs[idx].target_addr = (uintptr_t)(xdl_sym(handle, symbol, NULL));
  if (__predict_false(0 == sh_safe_addrs[idx].target_addr)) return -1;
  sh_safe_addrs[idx].orig_addr = 0;
  return 0;
}

int sh_safe_init(void) {
  sh_safe_api_level = sh_util_get_api_level();

  void *handle = xdl_open("libc.so", XDL_DEFAULT);
  if (NULL == handle) return -1;

  int r = -1;
  if (__predict_false(0 != sh_safe_init_func(handle, "pthread_getspecific", SH_SAFE_IDX_PTHREAD_GETSPECIFIC)))
    goto end;
  if (__predict_false(0 != sh_safe_init_func(handle, "pthread_setspecific", SH_SAFE_IDX_PTHREAD_SETSPECIFIC)))
    goto end;
  if (__predict_false(0 != sh_safe_init_func(handle, "abort", SH_SAFE_IDX_ABORT))) goto end;
  r = 0;

end:
  xdl_close(handle);
  return r;
}

uintptr_t *sh_safe_get_orig_addr_addr(uintptr_t target_addr) {
  for (size_t i = 0; i < SH_SAFE_IDX_SZ; i++) {
    if (sh_safe_addrs[i].target_addr == target_addr) {
      return &sh_safe_addrs[i].orig_addr;
    }
  }
  return NULL;
}

static uintptr_t sh_safe_get_orig_addr(size_t idx) {
  sh_safe_addr_t *addr = &sh_safe_addrs[idx];
  return 0 != addr->orig_addr ? addr->orig_addr : addr->target_addr;
}

void *sh_safe_pthread_getspecific(pthread_key_t key) {
  uintptr_t addr = sh_safe_get_orig_addr(SH_SAFE_IDX_PTHREAD_GETSPECIFIC);
  return ((void *(*)(pthread_key_t))addr)(key);
}

int sh_safe_pthread_setspecific(pthread_key_t key, const void *value) {
  if (sh_safe_api_level >= __ANDROID_API_M__) {
    uintptr_t addr = sh_safe_get_orig_addr(SH_SAFE_IDX_PTHREAD_SETSPECIFIC);
    return ((int (*)(pthread_key_t, const void *))addr)(key, value);
  } else {
    // Before Android M, pthread_setspecific() will call pthread_mutex_lock() and
    // pthread_mutex_unlock(). So if we use pthread_setspecific() in hub's trampo,
    // we will NOT be able to hook pthread_mutex_lock() and pthread_mutex_unlock().
    void **tls;
#if defined(__aarch64__)
    __asm__("mrs %0, tpidr_el0" : "=r"(tls));
#elif defined(__arm__)
    __asm__("mrc p15, 0, %0, c13, c0, 3" : "=r"(tls));
#endif
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-qual"
    tls[key] = (void *)value;
#pragma clang diagnostic pop
    return 0;
  }
}

void sh_safe_abort(void) {
  uintptr_t addr = sh_safe_get_orig_addr(SH_SAFE_IDX_ABORT);
  ((void (*)(void))addr)();
}

void *sh_safe_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
  return sys_mmap(addr, length, prot, flags, fd, offset);
}

int sh_safe_prctl(int option, unsigned long arg2, unsigned long arg3, unsigned long arg4,
                  unsigned long arg5) {
  return sys_prctl(option, arg2, arg3, arg4, arg5);
}
