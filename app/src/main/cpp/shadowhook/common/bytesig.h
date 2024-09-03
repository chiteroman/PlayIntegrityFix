// Copyright (c) 2021-2024 ByteDance Inc.
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

// version 1.0.4

/*
 * #include "bytesig.h"
 *
 * void init_once(void)
 * {
 *     bytesig_init(SIGSEGV);
 *     bytesig_init(SIGBUS);
 *     bytesig_init(SIGILL);
 *     bytesig_init(SIGABRT);
 *     // ......
 * }
 *
 * void unstable_func(void)
 * {
 *     int *p = NULL;
 *
 *     //
 *     // usage 1
 *     //
 *     BYTESIG_TRY(SIGSEGV, SIGBUS)
 *     {
 *         *p = 1;
 *     }
 *     BYTESIG_CATCH(signum, code)
 *     {
 *         LOG("signum %d (code %d)", signum, code);
 *     }
 *     BYTESIG_EXIT
 *
 *     //
 *     // usage 2
 *     //
 *     BYTESIG_TRY(SIGSEGV, SIGBUS)
 *     {
 *         *p = 2;
 *     }
 *     BYTESIG_CATCH(signum)
 *     {
 *         LOG("signum %d", signum);
 *     }
 *     BYTESIG_EXIT
 *
 *     //
 *     // usage 3
 *     //
 *     BYTESIG_TRY(SIGILL)
 *     {
 *         func_maybe_illed();
 *     }
 *     BYTESIG_CATCH()
 *     {
 *         do_something();
 *     }
 *     BYTESIG_EXIT
 *
 *     //
 *     // usage 4
 *     //
 *     BYTESIG_TRY(SIGABRT)
 *     {
 *         func_maybe_aborted();
 *     }
 *     BYTESIG_EXIT
 * }
 */

#ifndef BYTEDANCE_BYTESIG
#define BYTEDANCE_BYTESIG

#include <setjmp.h>
#include <signal.h>
#include <stddef.h>
#include <sys/syscall.h>
#include <unistd.h>

#define BYTESIG_TRY(...)                                                                                   \
  do {                                                                                                     \
    pid_t _bytesig_tid_ = gettid();                                                                        \
    if (0 == _bytesig_tid_) _bytesig_tid_ = (pid_t)syscall(SYS_gettid);                                    \
    sigjmp_buf _bytesig_jbuf_;                                                                             \
    int _bytesig_sigs_[] = {__VA_ARGS__};                                                                  \
    bytesig_protect(_bytesig_tid_, &_bytesig_jbuf_, _bytesig_sigs_, sizeof(_bytesig_sigs_) / sizeof(int)); \
    int _bytesig_protected_ = 1;                                                                           \
    int _bytesig_ex_ = sigsetjmp(_bytesig_jbuf_, 1);                                                       \
    if (0 == _bytesig_ex_) {
#define BYTESIG_CATCH_2(signum_, code_)                                                     \
  }                                                                                         \
  else {                                                                                    \
    bytesig_unprotect(_bytesig_tid_, _bytesig_sigs_, sizeof(_bytesig_sigs_) / sizeof(int)); \
    _bytesig_protected_ = 0;                                                                \
    int signum_ = (int)(((unsigned int)_bytesig_ex_ & 0xFF0000U) >> 16U);                   \
    int code_ = 0;                                                                          \
    if (((unsigned int)_bytesig_ex_ & 0xFF00U) > 0)                                         \
      code_ = (int)(((unsigned int)_bytesig_ex_ & 0xFF00U) >> 8U);                          \
    else if (((unsigned int)_bytesig_ex_ & 0xFFU) > 0)                                      \
      code_ = -((int)((unsigned int)_bytesig_ex_ & 0xFFU));                                 \
    (void)signum_;                                                                          \
    (void)code_;

#define BYTESIG_CATCH_1(signum_) BYTESIG_CATCH_2(signum_, _bytesig_code_)
#define BYTESIG_CATCH_0()        BYTESIG_CATCH_1(_bytesig_signum_)

#define FUNC_CHOOSER(_f1, _f2, _f3, ...)     _f3
#define FUNC_RECOMPOSER(argsWithParentheses) FUNC_CHOOSER argsWithParentheses
#define CHOOSE_FROM_ARG_COUNT(...)           FUNC_RECOMPOSER((__VA_ARGS__, BYTESIG_CATCH_2, BYTESIG_CATCH_1, ))
#define NO_ARG_EXPANDER()                    , , BYTESIG_CATCH_0
#define MACRO_CHOOSER(...)                   CHOOSE_FROM_ARG_COUNT(NO_ARG_EXPANDER __VA_ARGS__())

#define BYTESIG_CATCH(...) MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

#define BYTESIG_EXIT                                                                        \
  }                                                                                         \
  if (1 == _bytesig_protected_)                                                             \
    bytesig_unprotect(_bytesig_tid_, _bytesig_sigs_, sizeof(_bytesig_sigs_) / sizeof(int)); \
  }                                                                                         \
  while (0)                                                                                 \
    ;

#ifdef __cplusplus
extern "C" {
#endif

int bytesig_init(int signum);

void bytesig_protect(pid_t tid, sigjmp_buf *jbuf, const int signums[], size_t signums_cnt);
void bytesig_unprotect(pid_t tid, const int signums[], size_t signums_cnt);

#ifdef __cplusplus
}
#endif

#endif
