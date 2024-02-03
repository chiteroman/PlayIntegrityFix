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

#include "sh_errno.h"

#include <pthread.h>
#include <stdbool.h>

#include "shadowhook.h"

static int sh_errno_global = SHADOWHOOK_ERRNO_UNINIT;
static pthread_key_t sh_errno_tls_key;

int sh_errno_init(void) {
  if (__predict_false(0 != pthread_key_create(&sh_errno_tls_key, NULL))) {
    sh_errno_global = SHADOWHOOK_ERRNO_INIT_ERRNO;
    return -1;
  }
  sh_errno_global = SHADOWHOOK_ERRNO_OK;
  return 0;
}

void sh_errno_reset(void) {
  sh_errno_set(0);
}

void sh_errno_set(int error_number) {
  if (sh_errno_global == SHADOWHOOK_ERRNO_UNINIT || sh_errno_global == SHADOWHOOK_ERRNO_INIT_ERRNO) return;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wint-to-void-pointer-cast"
  pthread_setspecific(sh_errno_tls_key, (void *)error_number);
#pragma clang diagnostic pop
}

int sh_errno_get(void) {
  if (sh_errno_global == SHADOWHOOK_ERRNO_UNINIT || sh_errno_global == SHADOWHOOK_ERRNO_INIT_ERRNO)
    return sh_errno_global;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvoid-pointer-to-int-cast"
  return (int)(pthread_getspecific(sh_errno_tls_key));
#pragma clang diagnostic pop
}

const char *sh_errno_to_errmsg(int error_number) {
  static const char *msg[] = {/* 0  */ "OK",
                              /* 1  */ "Pending task",
                              /* 2  */ "Not initialized",
                              /* 3  */ "Invalid argument",
                              /* 4  */ "Out of memory",
                              /* 5  */ "MProtect failed",
                              /* 6  */ "Write to arbitrary address crashed",
                              /* 7  */ "Init errno mod failed",
                              /* 8  */ "Init bytesig SIGSEGV mod failed",
                              /* 9  */ "Init bytesig SIGBUS mod failed",
                              /* 10 */ "Init enter mod failed",
                              /* 11 */ "Init safe mod failed",
                              /* 12 */ "Init linker mod failed",
                              /* 13 */ "Init hub mod failed",
                              /* 14 */ "Create hub failed",
                              /* 15 */ "Monitor dlopen failed",
                              /* 16 */ "Create monitor thread failed",
                              /* 17 */ "Open ELF crashed",
                              /* 18 */ "Find symbol in ELF failed",
                              /* 19 */ "Find symbol in ELF crashed",
                              /* 20 */ "Duplicate hook",
                              /* 21 */ "Dladdr crashed",
                              /* 22 */ "Find dlinfo failed",
                              /* 23 */ "Symbol size too small",
                              /* 24 */ "Alloc enter failed",
                              /* 25 */ "Instruction rewrite crashed",
                              /* 26 */ "Instruction rewrite failed",
                              /* 27 */ "Switch not found",
                              /* 28 */ "Verify original instruction crashed",
                              /* 29 */ "Verify original instruction failed",
                              /* 30 */ "Exit instruction mismatch",
                              /* 31 */ "Free exit crashed",
                              /* 32 */ "Unhook on an error status task",
                              /* 33 */ "Unhook on an unfinished task",
                              /* 34 */ "ELF with an unsupported architecture",
                              /* 35 */ "Linker with an unsupported architecture"};

  if (error_number < 0 || error_number >= (int)(sizeof(msg) / sizeof(msg[0]))) return "Unknown error number";

  return msg[error_number];
}
