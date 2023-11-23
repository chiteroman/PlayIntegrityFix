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

#include "sh_linker.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "sh_log.h"
#include "sh_recorder.h"
#include "sh_sig.h"
#include "sh_switch.h"
#include "sh_util.h"
#include "shadowhook.h"
#include "xdl.h"

#ifndef __LP64__
#define SH_LINKER_BASENAME "linker"
#else
#define SH_LINKER_BASENAME "linker64"
#endif

#define SH_LINKER_SYM_G_DL_MUTEX  "__dl__ZL10g_dl_mutex"
#define SH_LINKER_SYM_DO_DLOPEN_L "__dl__Z9do_dlopenPKciPK17android_dlextinfo"
#define SH_LINKER_SYM_DO_DLOPEN_N "__dl__Z9do_dlopenPKciPK17android_dlextinfoPv"
#define SH_LINKER_SYM_DO_DLOPEN_O "__dl__Z9do_dlopenPKciPK17android_dlextinfoPKv"

static bool sh_linker_dlopen_hooked = false;

static sh_linker_post_dlopen_t sh_linker_post_dlopen;
static void *sh_linker_post_dlopen_arg;

static pthread_mutex_t *sh_linker_g_dl_mutex;
static uintptr_t sh_linker_dlopen_addr;  // save address of dlopen(==4.x) or do_dlopen(>=5.0)
static xdl_info_t sh_linker_dlopen_dlinfo;

#if defined(__arm__) && __ANDROID_API__ < __ANDROID_API_L__
static uintptr_t sh_linker_dlfcn[6];
static const char *sh_linker_dlfcn_name[6] = {"dlopen", "dlerror", "dlsym",
                                              "dladdr", "dlclose", "dl_unwind_find_exidx"};
#endif

__attribute__((constructor)) static void sh_linker_ctor(void) {
  sh_linker_dlopen_addr = (uintptr_t)dlopen;
#if defined(__arm__) && __ANDROID_API__ < __ANDROID_API_L__
  sh_linker_dlfcn[0] = (uintptr_t)dlopen;
  sh_linker_dlfcn[1] = (uintptr_t)dlerror;
  sh_linker_dlfcn[2] = (uintptr_t)dlsym;
  sh_linker_dlfcn[3] = (uintptr_t)dladdr;
  sh_linker_dlfcn[4] = (uintptr_t)dlclose;
  sh_linker_dlfcn[5] = (uintptr_t)dl_unwind_find_exidx;
#endif
}

static void *sh_linker_get_base_addr(xdl_info_t *dlinfo) {
  uintptr_t vaddr_min = UINTPTR_MAX;
  for (size_t i = 0; i < dlinfo->dlpi_phnum; i++) {
    const ElfW(Phdr) *phdr = &(dlinfo->dlpi_phdr[i]);
    if (PT_LOAD == phdr->p_type && vaddr_min > phdr->p_vaddr) vaddr_min = phdr->p_vaddr;
  }

  if (UINTPTR_MAX == vaddr_min)
    return dlinfo->dli_fbase;  // should not happen
  else
    return (void *)((uintptr_t)dlinfo->dli_fbase + SH_UTIL_PAGE_START(vaddr_min));
}

static bool sh_linker_check_arch(xdl_info_t *dlinfo) {
  ElfW(Ehdr) *ehdr = (ElfW(Ehdr) *)sh_linker_get_base_addr(dlinfo);

#if defined(__LP64__)
#define SH_LINKER_ELF_CLASS   ELFCLASS64
#define SH_LINKER_ELF_MACHINE EM_AARCH64
#else
#define SH_LINKER_ELF_CLASS   ELFCLASS32
#define SH_LINKER_ELF_MACHINE EM_ARM
#endif

  if (0 != memcmp(ehdr->e_ident, ELFMAG, SELFMAG)) return false;
  if (SH_LINKER_ELF_CLASS != ehdr->e_ident[EI_CLASS]) return false;
  if (SH_LINKER_ELF_MACHINE != ehdr->e_machine) return false;

  return true;
}

int sh_linker_init(void) {
  memset(&sh_linker_dlopen_dlinfo, 0, sizeof(sh_linker_dlopen_dlinfo));

  int api_level = sh_util_get_api_level();
  if (__predict_true(api_level >= __ANDROID_API_L__)) {
    sh_linker_dlopen_addr = 0;

    void *handle = xdl_open(SH_LINKER_BASENAME, XDL_DEFAULT);
    if (__predict_false(NULL == handle)) return -1;
    xdl_info(handle, XDL_DI_DLINFO, (void *)&sh_linker_dlopen_dlinfo);
    sh_linker_dlopen_dlinfo.dli_fname = SH_LINKER_BASENAME;

    // get g_dl_mutex
    sh_linker_g_dl_mutex = (pthread_mutex_t *)(xdl_dsym(handle, SH_LINKER_SYM_G_DL_MUTEX, NULL));

    // get do_dlopen
    if (api_level >= __ANDROID_API_O__)
      sh_linker_dlopen_dlinfo.dli_sname = SH_LINKER_SYM_DO_DLOPEN_O;
    else if (api_level >= __ANDROID_API_N__)
      sh_linker_dlopen_dlinfo.dli_sname = SH_LINKER_SYM_DO_DLOPEN_N;
    else
      sh_linker_dlopen_dlinfo.dli_sname = SH_LINKER_SYM_DO_DLOPEN_L;
    sh_linker_dlopen_dlinfo.dli_saddr =
        xdl_dsym(handle, sh_linker_dlopen_dlinfo.dli_sname, &(sh_linker_dlopen_dlinfo.dli_ssize));
    sh_linker_dlopen_addr = (uintptr_t)sh_linker_dlopen_dlinfo.dli_saddr;

    xdl_close(handle);
  }

  return (0 != sh_linker_dlopen_addr && (0 != sh_linker_g_dl_mutex || api_level < __ANDROID_API_L__)) ? 0
                                                                                                      : -1;
}

const char *sh_linker_match_dlfcn(uintptr_t target_addr) {
#if defined(__arm__) && __ANDROID_API__ < __ANDROID_API_L__
  if (sh_util_get_api_level() < __ANDROID_API_L__)
    for (size_t i = 0; i < sizeof(sh_linker_dlfcn) / sizeof(sh_linker_dlfcn[0]); i++)
      if (sh_linker_dlfcn[i] == target_addr) return sh_linker_dlfcn_name[i];
#else
  (void)target_addr;
#endif

  return NULL;
}

bool sh_linker_need_to_hook_dlopen(uintptr_t target_addr) {
  return SHADOWHOOK_IS_UNIQUE_MODE && !sh_linker_dlopen_hooked && target_addr == sh_linker_dlopen_addr;
}

typedef void *(*sh_linker_proxy_dlopen_t)(const char *, int);
static sh_linker_proxy_dlopen_t sh_linker_orig_dlopen;
static void *sh_linker_proxy_dlopen(const char *filename, int flag) {
  void *handle;
  if (SHADOWHOOK_IS_SHARED_MODE)
    handle = SHADOWHOOK_CALL_PREV(sh_linker_proxy_dlopen, sh_linker_proxy_dlopen_t, filename, flag);
  else
    handle = sh_linker_orig_dlopen(filename, flag);

  if (NULL != handle) sh_linker_post_dlopen(sh_linker_post_dlopen_arg);

  if (SHADOWHOOK_IS_SHARED_MODE) SHADOWHOOK_POP_STACK();
  return handle;
}

typedef void *(*sh_linker_proxy_do_dlopen_l_t)(const char *, int, const void *);
static sh_linker_proxy_do_dlopen_l_t sh_linker_orig_do_dlopen_l;
static void *sh_linker_proxy_do_dlopen_l(const char *name, int flags, const void *extinfo) {
  void *handle;
  if (SHADOWHOOK_IS_SHARED_MODE)
    handle = SHADOWHOOK_CALL_PREV(sh_linker_proxy_do_dlopen_l, sh_linker_proxy_do_dlopen_l_t, name, flags,
                                  extinfo);
  else
    handle = sh_linker_orig_do_dlopen_l(name, flags, extinfo);

  if (NULL != handle) sh_linker_post_dlopen(sh_linker_post_dlopen_arg);

  if (SHADOWHOOK_IS_SHARED_MODE) SHADOWHOOK_POP_STACK();
  return handle;
}

typedef void *(*sh_linker_proxy_do_dlopen_n_t)(const char *, int, const void *, void *);
static sh_linker_proxy_do_dlopen_n_t sh_linker_orig_do_dlopen_n;
static void *sh_linker_proxy_do_dlopen_n(const char *name, int flags, const void *extinfo,
                                         void *caller_addr) {
  void *handle;
  if (SHADOWHOOK_IS_SHARED_MODE)
    handle = SHADOWHOOK_CALL_PREV(sh_linker_proxy_do_dlopen_n, sh_linker_proxy_do_dlopen_n_t, name, flags,
                                  extinfo, caller_addr);
  else
    handle = sh_linker_orig_do_dlopen_n(name, flags, extinfo, caller_addr);

  if (NULL != handle) sh_linker_post_dlopen(sh_linker_post_dlopen_arg);

  if (SHADOWHOOK_IS_SHARED_MODE) SHADOWHOOK_POP_STACK();
  return handle;
}

int sh_linker_hook_dlopen(sh_linker_post_dlopen_t post_dlopen, void *post_dlopen_arg) {
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
  static int result = SHADOWHOOK_ERRNO_MONITOR_DLOPEN;

  if (sh_linker_dlopen_hooked) return result;
  pthread_mutex_lock(&lock);
  if (sh_linker_dlopen_hooked) goto end;

  // try hooking-dlopen only once
  sh_linker_dlopen_hooked = true;

  // do init for SHARED mode
  if (SHADOWHOOK_IS_SHARED_MODE)
    if (0 != sh_linker_init()) goto end;

  // save post callback ptr before hooking
  sh_linker_post_dlopen = post_dlopen;
  sh_linker_post_dlopen_arg = post_dlopen_arg;

  // hook for dlopen() or do_dlopen()
  int (*hook)(uintptr_t, uintptr_t, uintptr_t *, size_t *, xdl_info_t *) =
      SHADOWHOOK_IS_SHARED_MODE ? sh_switch_hook : sh_switch_hook_invisible;
  int api_level = sh_util_get_api_level();
  size_t backup_len = 0;
  int r;
  if (api_level < __ANDROID_API_L__) {
    // get & check dlinfo
    r = sh_linker_get_dlinfo_by_addr((void *)sh_linker_dlopen_addr, &sh_linker_dlopen_dlinfo, NULL, 0, NULL,
                                     0, false);
    if (SHADOWHOOK_ERRNO_LINKER_ARCH_MISMATCH == r) result = SHADOWHOOK_ERRNO_LINKER_ARCH_MISMATCH;
    if (0 != r) goto end;

    // hook
    r = hook(sh_linker_dlopen_addr, (uintptr_t)sh_linker_proxy_dlopen, (uintptr_t *)&sh_linker_orig_dlopen,
             &backup_len, &sh_linker_dlopen_dlinfo);

    // record
    sh_recorder_add_hook(r, true, sh_linker_dlopen_addr, SH_LINKER_BASENAME, "dlopen",
                         (uintptr_t)sh_linker_proxy_dlopen, backup_len, UINTPTR_MAX,
                         (uintptr_t)(__builtin_return_address(0)));

    if (0 != r) goto end;
  } else {
    // check dlinfo
    if (!sh_linker_check_arch(&sh_linker_dlopen_dlinfo)) {
      result = SHADOWHOOK_ERRNO_LINKER_ARCH_MISMATCH;
      goto end;
    }

    uintptr_t proxy;
    uintptr_t *orig;
    if (api_level >= __ANDROID_API_N__) {
      proxy = (uintptr_t)sh_linker_proxy_do_dlopen_n;
      orig = (uintptr_t *)&sh_linker_orig_do_dlopen_n;
    } else {
      proxy = (uintptr_t)sh_linker_proxy_do_dlopen_l;
      orig = (uintptr_t *)&sh_linker_orig_do_dlopen_l;
    }

    // hook
    pthread_mutex_lock(sh_linker_g_dl_mutex);
    r = hook(sh_linker_dlopen_addr, proxy, orig, &backup_len, &sh_linker_dlopen_dlinfo);
    pthread_mutex_unlock(sh_linker_g_dl_mutex);

    // record
    sh_recorder_add_hook(r, true, sh_linker_dlopen_addr, SH_LINKER_BASENAME,
                         sh_linker_dlopen_dlinfo.dli_sname, proxy, backup_len, UINTPTR_MAX,
                         (uintptr_t)(__builtin_return_address(0)));

    if (0 != r) goto end;
  }

  // OK
  result = 0;

end:
  pthread_mutex_unlock(&lock);
  SH_LOG_INFO("linker: hook dlopen %s, return: %d", 0 == result ? "OK" : "FAILED", result);
  return result;
}

int sh_linker_get_dlinfo_by_addr(void *addr, xdl_info_t *dlinfo, char *lib_name, size_t lib_name_sz,
                                 char *sym_name, size_t sym_name_sz, bool ignore_symbol_check) {
  // dladdr()
  bool crashed = false;
  void *dlcache = NULL;
  int r = 0;
  if (sh_util_get_api_level() >= __ANDROID_API_L__) {
    r = xdl_addr((void *)addr, dlinfo, &dlcache);
  } else {
    SH_SIG_TRY(SIGSEGV, SIGBUS) {
      r = xdl_addr((void *)addr, dlinfo, &dlcache);
    }
    SH_SIG_CATCH() {
      crashed = true;
    }
    SH_SIG_EXIT
  }
  SH_LOG_INFO("task: get dlinfo by target addr: target_addr %p, sym_name %s, sym_sz %zu, load_bias %" PRIxPTR
              ", pathname %s",
              addr, NULL == dlinfo->dli_sname ? "(NULL)" : dlinfo->dli_sname, dlinfo->dli_ssize,
              (uintptr_t)dlinfo->dli_fbase, NULL == dlinfo->dli_fname ? "(NULL)" : dlinfo->dli_fname);

  // check error
  if (crashed) {
    r = SHADOWHOOK_ERRNO_HOOK_DLADDR_CRASH;
    goto end;
  }
  if (0 == r || NULL == dlinfo->dli_fname) {
    r = SHADOWHOOK_ERRNO_HOOK_DLINFO;
    goto end;
  }
  if (!sh_linker_check_arch(dlinfo)) {
    r = SHADOWHOOK_ERRNO_ELF_ARCH_MISMATCH;
    goto end;
  }

  if (NULL == dlinfo->dli_sname) {
    if (ignore_symbol_check) {
      dlinfo->dli_saddr = addr;
      dlinfo->dli_sname = "unknown";
      dlinfo->dli_ssize = 1024;  // big enough
    } else {
      const char *matched_dlfcn_name = NULL;
      if (NULL == (matched_dlfcn_name = sh_linker_match_dlfcn((uintptr_t)addr))) {
        r = SHADOWHOOK_ERRNO_HOOK_DLINFO;
        goto end;
      } else {
        dlinfo->dli_saddr = addr;
        dlinfo->dli_sname = matched_dlfcn_name;
        dlinfo->dli_ssize = 4;  // safe length, only relative jumps are allowed
        SH_LOG_INFO("task: match dlfcn, target_addr %p, sym_name %s", addr, matched_dlfcn_name);
      }
    }
  }
  if (0 == dlinfo->dli_ssize) {
    r = SHADOWHOOK_ERRNO_HOOK_SYMSZ;
    goto end;
  }

  if (NULL != lib_name) strlcpy(lib_name, dlinfo->dli_fname, lib_name_sz);
  if (NULL != sym_name) strlcpy(sym_name, dlinfo->dli_sname, sym_name_sz);
  r = 0;

end:
  xdl_addr_clean(&dlcache);
  return r;
}

int sh_linker_get_dlinfo_by_sym_name(const char *lib_name, const char *sym_name, xdl_info_t *dlinfo,
                                     char *real_lib_name, size_t real_lib_name_sz) {
  // open library
  bool crashed = false;
  void *handle = NULL;
  if (sh_util_get_api_level() >= __ANDROID_API_L__) {
    handle = xdl_open(lib_name, XDL_DEFAULT);
  } else {
    SH_SIG_TRY(SIGSEGV, SIGBUS) {
      handle = xdl_open(lib_name, XDL_DEFAULT);
    }
    SH_SIG_CATCH() {
      crashed = true;
    }
    SH_SIG_EXIT
  }
  if (crashed) return SHADOWHOOK_ERRNO_HOOK_DLOPEN_CRASH;
  if (NULL == handle) return SHADOWHOOK_ERRNO_PENDING;

  // get dlinfo
  xdl_info(handle, XDL_DI_DLINFO, (void *)dlinfo);

  // check error
  if (!sh_linker_check_arch(dlinfo)) {
    xdl_close(handle);
    return SHADOWHOOK_ERRNO_ELF_ARCH_MISMATCH;
  }

  // lookup symbol address
  crashed = false;
  void *addr = NULL;
  size_t sym_size = 0;
  SH_SIG_TRY(SIGSEGV, SIGBUS) {
    // do xdl_sym() or xdl_dsym() in an dlclosed-ELF will cause a crash
    addr = xdl_sym(handle, sym_name, &sym_size);
    if (NULL == addr) addr = xdl_dsym(handle, sym_name, &sym_size);
  }
  SH_SIG_CATCH() {
    crashed = true;
  }
  SH_SIG_EXIT

  // close library
  xdl_close(handle);

  if (crashed) return SHADOWHOOK_ERRNO_HOOK_DLSYM_CRASH;
  if (NULL == addr) return SHADOWHOOK_ERRNO_HOOK_DLSYM;

  dlinfo->dli_fname = lib_name;
  dlinfo->dli_sname = sym_name;
  dlinfo->dli_saddr = addr;
  dlinfo->dli_ssize = sym_size;
  if (NULL != real_lib_name) strlcpy(real_lib_name, dlinfo->dli_fname, real_lib_name_sz);
  return 0;
}
