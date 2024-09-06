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

// ByteHook is now credited to all its contributors.
//
// Before ByteHook was open sourced in August 2021, it was designed, implemented and
// improved by the ByteHook Development Team from ByteDance IES, GIP and Client Infra.
// The team members (alphabetical order):
//
// Hongkai Liu (liuhongkai@bytedance.com)
// Jinshi Song (songjinshi@bytedance.com)
// Kelun Cai (caikelun@bytedance.com)
// Li Han (hanli.lee@bytedance.com)
// Li Zhang (zhangli.foxleezh@bytedance.com)
// Minghao Li (liminghao.mingo@bytedance.com)
// Nian Sun (sunnian@bytedance.com)
// Quanfei Li (liquanfei@bytedance.com)
// Shujie Wang (wangshujie.matt@bytedance.com)
// Tianzhou Shen (shentianzhou@bytedance.com)
// Xiangyu Pang (pangxiangyu@bytedance.com)
// Yingmin Piao (piaoyingmin@bytedance.com)
// Yonggang Sun (sunyonggang@bytedance.com)
// Zhi Guo (guozhi.kevin@bytedance.com)
// Zhi Xu (xuzhi@bytedance.com)
//

#ifndef BYTEDANCE_BYTEHOOK_H
#define BYTEDANCE_BYTEHOOK_H 1

#include <stdbool.h>
#include <stdint.h>

#define BYTEHOOK_VERSION "1.0.10"

#define BYTEHOOK_STATUS_CODE_OK                  0
#define BYTEHOOK_STATUS_CODE_UNINIT              1
#define BYTEHOOK_STATUS_CODE_INITERR_INVALID_ARG 2
#define BYTEHOOK_STATUS_CODE_INITERR_SYM         3
#define BYTEHOOK_STATUS_CODE_INITERR_TASK        4
#define BYTEHOOK_STATUS_CODE_INITERR_HOOK        5
#define BYTEHOOK_STATUS_CODE_INITERR_ELF         6
#define BYTEHOOK_STATUS_CODE_INITERR_ELF_REFR    7
#define BYTEHOOK_STATUS_CODE_INITERR_TRAMPO      8
#define BYTEHOOK_STATUS_CODE_INITERR_SIG         9
#define BYTEHOOK_STATUS_CODE_INITERR_DLMTR       10
#define BYTEHOOK_STATUS_CODE_INVALID_ARG         11
#define BYTEHOOK_STATUS_CODE_UNMATCH_ORIG_FUNC   12
#define BYTEHOOK_STATUS_CODE_NOSYM               13
#define BYTEHOOK_STATUS_CODE_GET_PROT            14
#define BYTEHOOK_STATUS_CODE_SET_PROT            15
#define BYTEHOOK_STATUS_CODE_SET_GOT             16
#define BYTEHOOK_STATUS_CODE_NEW_TRAMPO          17
#define BYTEHOOK_STATUS_CODE_APPEND_TRAMPO       18
#define BYTEHOOK_STATUS_CODE_GOT_VERIFY          19
#define BYTEHOOK_STATUS_CODE_REPEATED_FUNC       20
#define BYTEHOOK_STATUS_CODE_READ_ELF            21
#define BYTEHOOK_STATUS_CODE_CFI_HOOK_FAILED     22
#define BYTEHOOK_STATUS_CODE_ORIG_ADDR           23
#define BYTEHOOK_STATUS_CODE_INITERR_CFI         24
#define BYTEHOOK_STATUS_CODE_IGNORE              25
#define BYTEHOOK_STATUS_CODE_MAX                 255

#define BYTEHOOK_MODE_AUTOMATIC 0
#define BYTEHOOK_MODE_MANUAL    1

#ifdef __cplusplus
extern "C" {
#endif

const char *bytehook_get_version(void);

typedef void *bytehook_stub_t;

typedef void (*bytehook_hooked_t)(bytehook_stub_t task_stub, int status_code, const char *caller_path_name,
                                  const char *sym_name, void *new_func, void *prev_func, void *arg);

typedef bool (*bytehook_caller_allow_filter_t)(const char *caller_path_name, void *arg);

int bytehook_init(int mode, bool debug);

bytehook_stub_t bytehook_hook_single(const char *caller_path_name, const char *callee_path_name,
                                     const char *sym_name, void *new_func, bytehook_hooked_t hooked,
                                     void *hooked_arg);

bytehook_stub_t bytehook_hook_partial(bytehook_caller_allow_filter_t caller_allow_filter,
                                      void *caller_allow_filter_arg, const char *callee_path_name,
                                      const char *sym_name, void *new_func, bytehook_hooked_t hooked,
                                      void *hooked_arg);

bytehook_stub_t bytehook_hook_all(const char *callee_path_name, const char *sym_name, void *new_func,
                                  bytehook_hooked_t hooked, void *hooked_arg);

int bytehook_unhook(bytehook_stub_t stub);

int bytehook_add_ignore(const char *caller_path_name);

int bytehook_get_mode(void);
bool bytehook_get_debug(void);
void bytehook_set_debug(bool debug);
bool bytehook_get_recordable(void);
void bytehook_set_recordable(bool recordable);

// get operation records
#define BYTEHOOK_RECORD_ITEM_ALL             0xFF  // 0b11111111
#define BYTEHOOK_RECORD_ITEM_TIMESTAMP       (1 << 0)
#define BYTEHOOK_RECORD_ITEM_CALLER_LIB_NAME (1 << 1)
#define BYTEHOOK_RECORD_ITEM_OP              (1 << 2)
#define BYTEHOOK_RECORD_ITEM_LIB_NAME        (1 << 3)
#define BYTEHOOK_RECORD_ITEM_SYM_NAME        (1 << 4)
#define BYTEHOOK_RECORD_ITEM_NEW_ADDR        (1 << 5)
#define BYTEHOOK_RECORD_ITEM_ERRNO           (1 << 6)
#define BYTEHOOK_RECORD_ITEM_STUB            (1 << 7)
char *bytehook_get_records(uint32_t item_flags);
void bytehook_dump_records(int fd, uint32_t item_flags);

// for internal use
void *bytehook_get_prev_func(void *func);

// for internal use
void bytehook_pop_stack(void *return_address);

// for internal use
void *bytehook_get_return_address(void);

typedef void (*bytehook_pre_dlopen_t)(const char *filename, void *data);

typedef void (*bytehook_post_dlopen_t)(const char *filename,
                                       int result,  // 0: OK  -1: Failed
                                       void *data);

void bytehook_add_dlopen_callback(bytehook_pre_dlopen_t pre, bytehook_post_dlopen_t post, void *data);

void bytehook_del_dlopen_callback(bytehook_pre_dlopen_t pre, bytehook_post_dlopen_t post, void *data);

#ifdef __cplusplus
}
#endif

// call previous function in hook-function
#ifdef __cplusplus
#define BYTEHOOK_CALL_PREV(func, ...) ((decltype(&(func)))bytehook_get_prev_func((void *)(func)))(__VA_ARGS__)
#else
#define BYTEHOOK_CALL_PREV(func, func_sig, ...) \
  ((func_sig)bytehook_get_prev_func((void *)(func)))(__VA_ARGS__)
#endif

// get return address in hook-function
#define BYTEHOOK_RETURN_ADDRESS()                                                          \
  ((void *)(BYTEHOOK_MODE_AUTOMATIC == bytehook_get_mode() ? bytehook_get_return_address() \
                                                           : __builtin_return_address(0)))

// pop stack in hook-function (for C/C++)
#define BYTEHOOK_POP_STACK()                                                                             \
  do {                                                                                                   \
    if (BYTEHOOK_MODE_AUTOMATIC == bytehook_get_mode()) bytehook_pop_stack(__builtin_return_address(0)); \
  } while (0)

// pop stack in hook-function (for C++ only)
#ifdef __cplusplus
class BytehookStackScope {
 public:
  BytehookStackScope(void *return_address) : return_address_(return_address) {}

  ~BytehookStackScope() {
    if (BYTEHOOK_MODE_AUTOMATIC == bytehook_get_mode()) bytehook_pop_stack(return_address_);
  }

 private:
  void *return_address_;
};
#define BYTEHOOK_STACK_SCOPE() BytehookStackScope bytehook_stack_scope_obj(__builtin_return_address(0))
#endif

#endif
