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

// shadowhook is now credited to all its contributors.
//
// Before shadowhook was open sourced in February 2022, it was developed by
// the following developers from ByteDance's App Health Team:
//
// Kelun Cai (caikelun@bytedance.com)
// Pengying Xu (xupengying@bytedance.com)
//

#ifndef BYTEDANCE_SHADOWHOOK_H
#define BYTEDANCE_SHADOWHOOK_H

#include <stdbool.h>
#include <stdint.h>

#define SHADOWHOOK_VERSION "1.0.10"

#define SHADOWHOOK_ERRNO_OK                     0
#define SHADOWHOOK_ERRNO_PENDING                1
#define SHADOWHOOK_ERRNO_UNINIT                 2
#define SHADOWHOOK_ERRNO_INVALID_ARG            3
#define SHADOWHOOK_ERRNO_OOM                    4
#define SHADOWHOOK_ERRNO_MPROT                  5
#define SHADOWHOOK_ERRNO_WRITE_CRASH            6
#define SHADOWHOOK_ERRNO_INIT_ERRNO             7
#define SHADOWHOOK_ERRNO_INIT_SIGSEGV           8
#define SHADOWHOOK_ERRNO_INIT_SIGBUS            9
#define SHADOWHOOK_ERRNO_INIT_ENTER             10
#define SHADOWHOOK_ERRNO_INIT_SAFE              11
#define SHADOWHOOK_ERRNO_INIT_LINKER            12
#define SHADOWHOOK_ERRNO_INIT_HUB               13
#define SHADOWHOOK_ERRNO_HUB_CREAT              14
#define SHADOWHOOK_ERRNO_MONITOR_DLOPEN         15
#define SHADOWHOOK_ERRNO_MONITOR_THREAD         16
#define SHADOWHOOK_ERRNO_HOOK_DLOPEN_CRASH      17
#define SHADOWHOOK_ERRNO_HOOK_DLSYM             18
#define SHADOWHOOK_ERRNO_HOOK_DLSYM_CRASH       19
#define SHADOWHOOK_ERRNO_HOOK_DUP               20
#define SHADOWHOOK_ERRNO_HOOK_DLADDR_CRASH      21
#define SHADOWHOOK_ERRNO_HOOK_DLINFO            22
#define SHADOWHOOK_ERRNO_HOOK_SYMSZ             23
#define SHADOWHOOK_ERRNO_HOOK_ENTER             24
#define SHADOWHOOK_ERRNO_HOOK_REWRITE_CRASH     25
#define SHADOWHOOK_ERRNO_HOOK_REWRITE_FAILED    26
#define SHADOWHOOK_ERRNO_UNHOOK_NOTFOUND        27
#define SHADOWHOOK_ERRNO_UNHOOK_CMP_CRASH       28
#define SHADOWHOOK_ERRNO_UNHOOK_TRAMPO_MISMATCH 29
#define SHADOWHOOK_ERRNO_UNHOOK_EXIT_MISMATCH   30
#define SHADOWHOOK_ERRNO_UNHOOK_EXIT_CRASH      31
#define SHADOWHOOK_ERRNO_UNHOOK_ON_ERROR        32
#define SHADOWHOOK_ERRNO_UNHOOK_ON_UNFINISHED   33
#define SHADOWHOOK_ERRNO_ELF_ARCH_MISMATCH      34
#define SHADOWHOOK_ERRNO_LINKER_ARCH_MISMATCH   35

#ifdef __cplusplus
extern "C" {
#endif

const char *shadowhook_get_version(void);

// init
typedef enum {
  SHADOWHOOK_MODE_SHARED = 0,  // a function can be hooked multiple times
  SHADOWHOOK_MODE_UNIQUE = 1   // a function can only be hooked once, and hooking again will report an error
} shadowhook_mode_t;
int shadowhook_init(shadowhook_mode_t mode, bool debuggable);
int shadowhook_get_init_errno(void);

// get and set attributes
#define SHADOWHOOK_IS_SHARED_MODE (SHADOWHOOK_MODE_SHARED == shadowhook_get_mode())
#define SHADOWHOOK_IS_UNIQUE_MODE (SHADOWHOOK_MODE_UNIQUE == shadowhook_get_mode())
shadowhook_mode_t shadowhook_get_mode(void);
bool shadowhook_get_debuggable(void);
void shadowhook_set_debuggable(bool debuggable);
bool shadowhook_get_recordable(void);
void shadowhook_set_recordable(bool recordable);

// get error-number and error message
int shadowhook_get_errno(void);
const char *shadowhook_to_errmsg(int error_number);

// hook and unhook
typedef void (*shadowhook_hooked_t)(int error_number, const char *lib_name, const char *sym_name,
                                    void *sym_addr, void *new_addr, void *orig_addr, void *arg);
void *shadowhook_hook_func_addr(void *func_addr, void *new_addr, void **orig_addr);
void *shadowhook_hook_sym_addr(void *sym_addr, void *new_addr, void **orig_addr);
void *shadowhook_hook_sym_name(const char *lib_name, const char *sym_name, void *new_addr, void **orig_addr);
void *shadowhook_hook_sym_name_callback(const char *lib_name, const char *sym_name, void *new_addr,
                                        void **orig_addr, shadowhook_hooked_t hooked, void *hooked_arg);
int shadowhook_unhook(void *stub);

// get operation records
#define SHADOWHOOK_RECORD_ITEM_ALL             0x3FF  // 0b1111111111
#define SHADOWHOOK_RECORD_ITEM_TIMESTAMP       (1 << 0)
#define SHADOWHOOK_RECORD_ITEM_CALLER_LIB_NAME (1 << 1)
#define SHADOWHOOK_RECORD_ITEM_OP              (1 << 2)
#define SHADOWHOOK_RECORD_ITEM_LIB_NAME        (1 << 3)
#define SHADOWHOOK_RECORD_ITEM_SYM_NAME        (1 << 4)
#define SHADOWHOOK_RECORD_ITEM_SYM_ADDR        (1 << 5)
#define SHADOWHOOK_RECORD_ITEM_NEW_ADDR        (1 << 6)
#define SHADOWHOOK_RECORD_ITEM_BACKUP_LEN      (1 << 7)
#define SHADOWHOOK_RECORD_ITEM_ERRNO           (1 << 8)
#define SHADOWHOOK_RECORD_ITEM_STUB            (1 << 9)
char *shadowhook_get_records(uint32_t item_flags);
void shadowhook_dump_records(int fd, uint32_t item_flags);

// helper functions for "get symbol-address from library-name and symbol-name"
void *shadowhook_dlopen(const char *lib_name);
void shadowhook_dlclose(void *handle);
void *shadowhook_dlsym(void *handle, const char *sym_name);
void *shadowhook_dlsym_dynsym(void *handle, const char *sym_name);
void *shadowhook_dlsym_symtab(void *handle, const char *sym_name);

// for internal use
void *shadowhook_get_prev_func(void *func);
void shadowhook_pop_stack(void *return_address);
void shadowhook_allow_reentrant(void *return_address);
void shadowhook_disallow_reentrant(void *return_address);
void *shadowhook_get_return_address(void);

#ifdef __cplusplus
}
#endif

// call previous function in proxy-function
#ifdef __cplusplus
#define SHADOWHOOK_CALL_PREV(func, ...) \
  ((decltype(&(func)))shadowhook_get_prev_func((void *)(func)))(__VA_ARGS__)
#else
#define SHADOWHOOK_CALL_PREV(func, func_sig, ...) \
  ((func_sig)shadowhook_get_prev_func((void *)(func)))(__VA_ARGS__)
#endif
// pop stack in proxy-function (for C/C++)
#define SHADOWHOOK_POP_STACK()                                                        \
  do {                                                                                \
    if (SHADOWHOOK_IS_SHARED_MODE) shadowhook_pop_stack(__builtin_return_address(0)); \
  } while (0)

// pop stack in proxy-function (for C++ only)
#ifdef __cplusplus
class ShadowhookStackScope {
 public:
  ShadowhookStackScope(void *return_address) : return_address_(return_address) {}
  ~ShadowhookStackScope() {
    if (SHADOWHOOK_IS_SHARED_MODE) shadowhook_pop_stack(return_address_);
  }

 private:
  void *return_address_;
};
#define SHADOWHOOK_STACK_SCOPE() ShadowhookStackScope shadowhook_stack_scope_obj(__builtin_return_address(0))
#endif

// allow reentrant of the current proxy-function
#define SHADOWHOOK_ALLOW_REENTRANT()                                                        \
  do {                                                                                      \
    if (SHADOWHOOK_IS_SHARED_MODE) shadowhook_allow_reentrant(__builtin_return_address(0)); \
  } while (0)

// disallow reentrant of the current proxy-function
#define SHADOWHOOK_DISALLOW_REENTRANT()                                                        \
  do {                                                                                         \
    if (SHADOWHOOK_IS_SHARED_MODE) shadowhook_disallow_reentrant(__builtin_return_address(0)); \
  } while (0)

// get return address in proxy-function
#define SHADOWHOOK_RETURN_ADDRESS() \
  ((void *)(SHADOWHOOK_IS_SHARED_MODE ? shadowhook_get_return_address() : __builtin_return_address(0)))

#endif
