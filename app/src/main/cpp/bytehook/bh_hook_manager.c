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

#include "bh_hook_manager.h"

#include <dlfcn.h>
#include <inttypes.h>
#include <link.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "bh_const.h"
#include "bh_core.h"
#include "bh_elf.h"
#include "bh_hook.h"
#include "bh_log.h"
#include "bh_task.h"
#include "bh_trampo.h"
#include "bh_util.h"
#include "bytesig.h"
#include "tree.h"

#define BH_HOOK_MANAGER_GOT_MAX_CAP 32

// RB-tree for ELF info (bh_elf_t)
static __inline__ int bh_hook_cmp(bh_hook_t *a, bh_hook_t *b) {
  if (a->got_addr == b->got_addr)
    return 0;
  else
    return a->got_addr > b->got_addr ? 1 : -1;
}
typedef RB_HEAD(bh_hook_tree, bh_hook) bh_hook_tree_t;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
RB_GENERATE_STATIC(bh_hook_tree, bh_hook, link, bh_hook_cmp)
#pragma clang diagnostic pop

struct bh_hook_manager {
  bh_hook_tree_t hooks;
  bh_hook_tree_t abandoned_hooks;
  pthread_mutex_t hooks_lock;
};

bh_hook_manager_t *bh_hook_manager_create(void) {
  bh_hook_manager_t *self;
  if (NULL == (self = malloc(sizeof(bh_hook_manager_t)))) return NULL;
  RB_INIT(&self->hooks);
  RB_INIT(&self->abandoned_hooks);
  pthread_mutex_init(&self->hooks_lock, NULL);
  return self;
}

static bh_hook_t *bh_hook_manager_find_hook(bh_hook_manager_t *self, void *got_addr) {
  bh_hook_t hook_key = {.got_addr = got_addr};
  return RB_FIND(bh_hook_tree, &self->hooks, &hook_key);
}

static bh_hook_t *bh_hook_manager_create_hook(bh_hook_manager_t *self, void *got_addr, void *orig_func,
                                              void **trampo) {
  // create hook chain
  bh_hook_t *hook = bh_hook_create(got_addr, orig_func);
  if (NULL == hook) return NULL;

  // create trampoline for the hook chain
  *trampo = bh_trampo_create(hook);
  if (NULL == *trampo) {
    bh_hook_destroy(&hook);
    return NULL;
  }

  // save the hook chain
  RB_INSERT(bh_hook_tree, &self->hooks, hook);

  BH_LOG_INFO("hook chain: created for GOT %" PRIxPTR ", orig func %" PRIxPTR, (uintptr_t)got_addr,
              (uintptr_t)orig_func);
  return hook;
}

static int bh_hook_manager_add_func(bh_hook_manager_t *self, bh_elf_t *caller_elf, void *got_addr,
                                    void *orig_func, bh_task_t *task, void **trampo, void **orig_func_ret) {
  *trampo = NULL;
  int r;

  pthread_mutex_lock(&self->hooks_lock);

  // find or create hook chain
  bh_hook_t *hook = bh_hook_manager_find_hook(self, got_addr);
  *orig_func_ret = (NULL == hook ? orig_func : hook->orig_func);
  if (NULL == hook) hook = bh_hook_manager_create_hook(self, got_addr, orig_func, trampo);
  if (NULL == hook) {
    bh_task_hooked(task, BYTEHOOK_STATUS_CODE_NEW_TRAMPO, caller_elf->pathname, orig_func);
    r = BYTEHOOK_STATUS_CODE_NEW_TRAMPO;
    goto end;
  }

  // add new-func to hook chain
  if (0 != (r = bh_hook_add_func(hook, task->new_func, task->id))) {
    bh_task_hooked(task, r, caller_elf->pathname, orig_func);
    goto end;
  }

  r = 0;  // OK

end:
  pthread_mutex_unlock(&self->hooks_lock);
  return r;
}

static int bh_hook_manager_del_func(bh_hook_manager_t *self, void *got_addr, bh_task_t *task,
                                    void **restore_func) {
  int r = -1;

  if (NULL != restore_func) *restore_func = NULL;

  pthread_mutex_lock(&self->hooks_lock);

  bh_hook_t *hook = bh_hook_manager_find_hook(self, got_addr);
  if (NULL == hook) goto end;

  bool useful = bh_hook_del_func(hook, task->new_func);
  if (!useful) {
    // move hook chain to abandoned-hooks set
    // we can't delete it, because other threads may be running on its trampoline
    RB_REMOVE(bh_hook_tree, &self->hooks, hook);
    RB_INSERT(bh_hook_tree, &self->abandoned_hooks, hook);

    if (NULL != restore_func) *restore_func = hook->orig_func;
  }

  r = 0;  // OK

end:
  pthread_mutex_unlock(&self->hooks_lock);
  return r;
}

static int bh_hook_manager_verify_got_value(bh_elf_t *caller_elf, bh_task_t *task, void *got_addr) {
  // check for GOT's address itself
  Dl_info info;
  if (0 == dladdr(got_addr, &info)) return -1;

  // check for GOT-value's address
  if (0 == dladdr(*((void **)got_addr), &info)) {
    // bypass for libdl.so
    if (bh_elf_is_match(caller_elf, BH_CONST_BASENAME_DL)) {
      BH_LOG_INFO("hook chain: verify bypass libdl.so: %s", task->sym_name);
      return 0;
    }

    // bypass for dl-functions
    if (0 == strcmp(task->sym_name, "dlopen") || 0 == strcmp(task->sym_name, "dlclose") ||
        0 == strcmp(task->sym_name, "dlsym") || 0 == strcmp(task->sym_name, "dlvsym") ||
        0 == strcmp(task->sym_name, "dladdr") || 0 == strcmp(task->sym_name, "dlerror") ||
        0 == strcmp(task->sym_name, "dl_iterate_phdr") ||
        0 == strcmp(task->sym_name, "dl_unwind_find_exidx") ||
        0 == strcmp(task->sym_name, "android_dlopen_ext") ||
        0 == strcmp(task->sym_name, "android_dlwarning") ||
        0 == strcmp(task->sym_name, "android_get_LD_LIBRARY_PATH") ||
        0 == strcmp(task->sym_name, "android_update_LD_LIBRARY_PATH") ||
        0 == strcmp(task->sym_name, "android_set_application_target_sdk_version") ||
        0 == strcmp(task->sym_name, "android_get_application_target_sdk_version") ||
        0 == strcmp(task->sym_name, "android_init_namespaces") ||
        0 == strcmp(task->sym_name, "android_create_namespace")) {
      BH_LOG_INFO("hook chain: verify bypass dl-functions: %s", task->sym_name);
      return 0;
    }

    return -1;
  }

  // check for normal export func
  if (NULL != info.dli_sname && 0 == strcmp(info.dli_sname, task->sym_name)) {
    BH_LOG_INFO("hook chain: verify OK: %s in %s", task->sym_name, info.dli_fname);
    return 0;
  }

  // get ELF
  if (NULL == info.dli_fname || '\0' == info.dli_fname[0]) return -1;
  bh_elf_t *callee_elf = bh_elf_manager_find_elf(bh_core_global()->elf_mgr, info.dli_fname);
  if (NULL == callee_elf) return -1;

  int r = -1;

  // bypass for ifunc
  if (NULL == info.dli_sname) {
    ElfW(Sym) *sym = bh_elf_find_export_func_symbol_by_symbol_name(callee_elf, task->sym_name);
    if (NULL != sym && STT_GNU_IFUNC == ELF_ST_TYPE(sym->st_info)) {
      BH_LOG_INFO("hook chain: verify bypass ifunc: %s in %s", task->sym_name, info.dli_fname);
      r = 0;
    }
  }
  // bypass for alias-func
  else {
    void *addr = bh_elf_find_export_func_addr_by_symbol_name(callee_elf, info.dli_sname);
    if (NULL != addr && addr == *((void **)got_addr)) {
      BH_LOG_INFO("hook chain: verify bypass alias-func: %s in %s", task->sym_name, info.dli_fname);
      r = 0;
    }
  }

  return r;
}

static int bh_hook_manager_replace_got_value(bh_elf_t *caller_elf, bh_task_t *task, void *got_addr,
                                             void *orig_func, void *new_func) {
  // verify the GOT value
  if (BH_TASK_STATUS_UNHOOKING != task->status) {
    if (0 != bh_hook_manager_verify_got_value(caller_elf, task, got_addr)) {
      bh_task_hooked(task, BYTEHOOK_STATUS_CODE_GOT_VERIFY, caller_elf->pathname, orig_func);
      return BYTEHOOK_STATUS_CODE_GOT_VERIFY;
    }
  }

  // get permission by address
  int prot = bh_elf_get_protect_by_addr(caller_elf, got_addr);
  if (0 == prot) {
    bh_task_hooked(task, BYTEHOOK_STATUS_CODE_GET_PROT, caller_elf->pathname, orig_func);
    return BYTEHOOK_STATUS_CODE_GET_PROT;
  }

  // add write permission
  if (0 == (prot & PROT_WRITE)) {
    if (0 != bh_util_set_addr_protect(got_addr, prot | PROT_WRITE)) {
      bh_task_hooked(task, BYTEHOOK_STATUS_CODE_SET_PROT, caller_elf->pathname, orig_func);
      return BYTEHOOK_STATUS_CODE_SET_PROT;
    }
  }

  // replace the target function address by "new_func"
  int r;
  BYTESIG_TRY(SIGSEGV, SIGBUS) {
    __atomic_store_n((uintptr_t *)got_addr, (uintptr_t)new_func, __ATOMIC_SEQ_CST);
    r = 0;
  }
  BYTESIG_CATCH() {
    bh_elf_set_error(caller_elf, true);
    bh_task_hooked(task, BYTEHOOK_STATUS_CODE_SET_GOT, caller_elf->pathname, orig_func);
    r = BYTEHOOK_STATUS_CODE_SET_GOT;
  }
  BYTESIG_EXIT

  // delete write permission
  if (0 == (prot & PROT_WRITE)) bh_util_set_addr_protect(got_addr, prot);

  return r;
}

static size_t bh_hook_manager_find_all_got(bh_elf_t *caller_elf, bh_task_t *task, void **addr_array,
                                           size_t addr_array_cap) {
  if (NULL == task->callee_addr) {
    // by import symbol name
    return bh_elf_find_import_func_addr_by_symbol_name(caller_elf, task->sym_name, addr_array,
                                                       addr_array_cap);
  } else {
    // by callee address
    return bh_elf_find_import_func_addr_by_callee_addr(caller_elf, task->callee_addr, addr_array,
                                                       addr_array_cap);
  }
}

static int bh_hook_manager_hook_single_got(bh_hook_manager_t *self, bh_elf_t *caller_elf, bh_task_t *task,
                                           void *got_addr, void **orig_func_ret) {
  int r;

  void *orig_func = NULL;
  BYTESIG_TRY(SIGSEGV, SIGBUS) {
    orig_func = *((void **)got_addr);
  }
  BYTESIG_CATCH() {
    bh_elf_set_error(caller_elf, true);
    bh_task_hooked(task, BYTEHOOK_STATUS_CODE_READ_ELF, caller_elf->pathname, orig_func);
    return BYTEHOOK_STATUS_CODE_SET_GOT;
  }
  BYTESIG_EXIT

  if (BYTEHOOK_MODE_MANUAL == bh_core_get_mode()) {
    // manual mode:

    // 1. always patch with the externally specified address
    r = bh_hook_manager_replace_got_value(caller_elf, task, got_addr, orig_func, task->new_func);

    // 2. save the original address in task object for unhook
    if (0 == r) {
      bh_task_set_manual_orig_func(task, orig_func);
      BH_LOG_INFO("hook chain: manual REPLACE. GOT %" PRIxPTR ": %" PRIxPTR " -> %" PRIxPTR ", %s, %s",
                  (uintptr_t)got_addr, (uintptr_t)orig_func, (uintptr_t)task->new_func, task->sym_name,
                  caller_elf->pathname);
    }

    // 3. return the original address
    if (0 == r) *orig_func_ret = orig_func;
  } else {
    // automatic mode:

    // 1. add new-func to the hook chain
    void *trampo = NULL;
    void *orig_func_real = NULL;
    r = bh_hook_manager_add_func(self, caller_elf, got_addr, orig_func, task, &trampo, &orig_func_real);

    // 2. replace with the trampoline address if we haven't done it yet
    if (0 == r && NULL != trampo) {
      r = bh_hook_manager_replace_got_value(caller_elf, task, got_addr, orig_func, trampo);
      if (0 == r) {
        BH_LOG_INFO("hook chain: auto REPLACE. GOT %" PRIxPTR ": %" PRIxPTR " -> %" PRIxPTR ", %s, %s",
                    (uintptr_t)got_addr, (uintptr_t)orig_func, (uintptr_t)trampo, task->sym_name,
                    caller_elf->pathname);
      } else {
        bh_hook_manager_del_func(self, got_addr, task, NULL);
      }
    }

    // 3. return the original address
    if (0 == r) *orig_func_ret = orig_func_real;
  }

  // done
  if (0 == r) {
    BH_LOG_INFO("hook chain: hook OK. GOT %" PRIxPTR ": + %" PRIxPTR ", %s, %s", (uintptr_t)got_addr,
                (uintptr_t)task->new_func, task->sym_name, caller_elf->pathname);
  }

  return r;
}

static int bh_hook_manager_unhook_single_got(bh_hook_manager_t *self, bh_elf_t *caller_elf, bh_task_t *task,
                                             void *got_addr) {
  int r = 0;
  void *restore_func;

  void *orig_func = NULL;
  BYTESIG_TRY(SIGSEGV, SIGBUS) {
    orig_func = *((void **)got_addr);
  }
  BYTESIG_CATCH() {
    bh_elf_set_error(caller_elf, true);
    bh_task_hooked(task, BYTEHOOK_STATUS_CODE_READ_ELF, caller_elf->pathname, orig_func);
    return BYTEHOOK_STATUS_CODE_SET_GOT;
  }
  BYTESIG_EXIT

  if (BYTEHOOK_MODE_MANUAL == bh_core_get_mode()) {
    // manual mode:
    restore_func = bh_task_get_manual_orig_func(task);
    if (NULL != restore_func) {
      r = bh_hook_manager_replace_got_value(caller_elf, task, got_addr, NULL, restore_func);
      if (0 == r) {
        BH_LOG_INFO("hook chain: manual RESTORE. GOT %" PRIxPTR ": %" PRIxPTR " -> %" PRIxPTR ", %s, %s",
                    (uintptr_t)got_addr, (uintptr_t)orig_func, (uintptr_t)restore_func, task->sym_name,
                    caller_elf->pathname);
      }
    }
  } else {
    // automatic mode:

    // 1. delete new-func in the hook chain
    r = bh_hook_manager_del_func(self, got_addr, task, &restore_func);

    // 2. restore GOT to original function if there is no enabled hook func
    if (0 == r && NULL != restore_func) {
      r = bh_hook_manager_replace_got_value(caller_elf, task, got_addr, NULL, restore_func);
      if (0 == r) {
        BH_LOG_INFO("hook chain: auto RESTORE. GOT %" PRIxPTR ": %" PRIxPTR " -> %" PRIxPTR ", %s, %s",
                    (uintptr_t)got_addr, (uintptr_t)orig_func, (uintptr_t)restore_func, task->sym_name,
                    caller_elf->pathname);
      }
    }
  }

  if (0 == r) {
    BH_LOG_INFO("hook chain: unhook OK. GOT %" PRIxPTR ": - %" PRIxPTR ", %s, %s", (uintptr_t)got_addr,
                (uintptr_t)task->new_func, task->sym_name, caller_elf->pathname);
  }

  return r;
}

static void bh_hook_manager_hook_impl(bh_hook_manager_t *self, bh_task_t *task, bh_elf_t *caller_elf) {
  // get all GOT entries
  void *addr_array[BH_HOOK_MANAGER_GOT_MAX_CAP];
  size_t addr_array_sz =
      bh_hook_manager_find_all_got(caller_elf, task, addr_array, BH_HOOK_MANAGER_GOT_MAX_CAP);
  if (0 == addr_array_sz) {
    if (BH_TASK_TYPE_SINGLE == task->type)
      bh_task_hooked(task, BYTEHOOK_STATUS_CODE_NOSYM, caller_elf->pathname, NULL);
    return;
  }

  // do callback with BYTEHOOK_STATUS_CODE_ORIG_ADDR for manual-mode
  //
  // In manual mode, the caller needs to save the original function address
  // in the hooked callback, and then may call the original function through
  // this address in the proxy function. So we need to execute the hooked callback
  // first, and then execute the address replacement in the GOT, otherwise it
  // will cause a crash due to timing issues.
  if (BYTEHOOK_MODE_MANUAL == bh_core_get_mode()) {
    void *orig_func_real = *((void **)(addr_array[0]));
    bh_task_hooked(task, BYTEHOOK_STATUS_CODE_ORIG_ADDR, caller_elf->pathname, orig_func_real);
  }

  // hook
  bool everything_ok = true;
  void *orig_func = NULL;
  bh_elf_hook_lock(caller_elf);
  for (size_t i = 0; i < addr_array_sz; i++)
    if (0 != bh_hook_manager_hook_single_got(self, caller_elf, task, addr_array[i], &orig_func))
      everything_ok = false;
  bh_elf_hook_unlock(caller_elf);

  // do callback with STATUS_CODE_OK only once
  if (everything_ok) bh_task_hooked(task, BYTEHOOK_STATUS_CODE_OK, caller_elf->pathname, orig_func);
}

#ifdef __LP64__
static void bh_hook_manager_cfi_slowpath(uint64_t CallSiteTypeId, void *Ptr) {
  (void)CallSiteTypeId, (void)Ptr;

  BYTEHOOK_POP_STACK();
}

static void bh_hook_manager_cfi_slowpath_diag(uint64_t CallSiteTypeId, void *Ptr, void *DiagData) {
  (void)CallSiteTypeId, (void)Ptr, (void)DiagData;

  BYTEHOOK_POP_STACK();
}

static void bh_hook_manager_cfi_hooked(bytehook_stub_t task_stub, int status_code,
                                       const char *caller_path_name, const char *sym_name, void *new_func,
                                       void *prev_func, void *arg) {
  (void)task_stub, (void)new_func, (void)prev_func;

  bool *ok = (bool *)arg;
  if (BYTEHOOK_STATUS_CODE_OK == status_code) {
    BH_LOG_INFO("hook cfi OK: %s, %s", caller_path_name, sym_name);
    *ok = true;
  }
  if (BYTEHOOK_STATUS_CODE_NOSYM == status_code) {
    BH_LOG_INFO("hook cfi NOSYM: %s, %s", caller_path_name, sym_name);
    *ok = true;
  }
}

static bool bh_hook_manager_hook_cfi(bh_hook_manager_t *self, bh_elf_t *caller_elf) {
  bool ok;

  ok = false;
  bh_task_t *task =
      bh_task_create_single(caller_elf->pathname, NULL, BH_CONST_SYM_CFI_SLOWPATH,
                            (void *)bh_hook_manager_cfi_slowpath, bh_hook_manager_cfi_hooked, (void *)&ok);
  if (NULL == task) return false;
  bh_hook_manager_hook_impl(self, task, caller_elf);
  bh_task_destroy(&task);
  if (!ok) return false;

  ok = false;
  task = bh_task_create_single(caller_elf->pathname, NULL, BH_CONST_SYM_CFI_SLOWPATH_DIAG,
                               (void *)bh_hook_manager_cfi_slowpath_diag, bh_hook_manager_cfi_hooked,
                               (void *)&ok);
  if (NULL == task) return false;
  bh_hook_manager_hook_impl(self, task, caller_elf);
  bh_task_destroy(&task);
  if (!ok) return false;

  return true;
}
#endif

void bh_hook_manager_hook(bh_hook_manager_t *self, bh_task_t *task, bh_elf_t *caller_elf) {
  // check ELF
  if (bh_elf_get_error(caller_elf)) {
    if (BH_TASK_TYPE_SINGLE == task->type)
      bh_task_hooked(task, BYTEHOOK_STATUS_CODE_READ_ELF, caller_elf->pathname, NULL);
    return;
  }

#ifdef __LP64__
  if (bh_util_get_api_level() >= __ANDROID_API_O__) {
    // hook __cfi_slowpath and __cfi_slowpath_diag (only once)
    if (!caller_elf->cfi_hooked) {
      bh_elf_cfi_hook_lock(caller_elf);
      if (!caller_elf->cfi_hooked) {
        caller_elf->cfi_hooked_ok = bh_hook_manager_hook_cfi(self, caller_elf);
        caller_elf->cfi_hooked = true;
      }
      bh_elf_cfi_hook_unlock(caller_elf);
    }

    // check CIF hook
    if (!caller_elf->cfi_hooked_ok) {
      if (BH_TASK_TYPE_SINGLE == task->type)
        bh_task_hooked(task, BYTEHOOK_STATUS_CODE_CFI_HOOK_FAILED, caller_elf->pathname, NULL);
      return;
    }
  }
#endif

  bh_hook_manager_hook_impl(self, task, caller_elf);
}

void bh_hook_manager_unhook(bh_hook_manager_t *self, bh_task_t *task, bh_elf_t *caller_elf) {
  // get all GOT entries
  void *addr_array[BH_HOOK_MANAGER_GOT_MAX_CAP];
  size_t addr_array_sz = bh_elf_find_import_func_addr_by_symbol_name(caller_elf, task->sym_name, addr_array,
                                                                     BH_HOOK_MANAGER_GOT_MAX_CAP);
  if (0 == addr_array_sz) return;

  // unhook
  bool everything_ok = true;
  bh_elf_hook_lock(caller_elf);
  for (size_t i = 0; i < addr_array_sz; i++)
    if (0 != bh_hook_manager_unhook_single_got(self, caller_elf, task, addr_array[i])) everything_ok = false;
  bh_elf_hook_unlock(caller_elf);

  // do callback with STATUS_CODE_OK only once
  if (everything_ok) bh_task_hooked(task, BYTEHOOK_STATUS_CODE_OK, caller_elf->pathname, NULL);
}
