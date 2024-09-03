// Copyright (c) 2020-2024 HexHacking Team
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

// Created by caikelun on 2021-02-21.

#include "xdl_linker.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>

#include "xdl.h"
#include "xdl_iterate.h"
#include "xdl_util.h"

#define XDL_LINKER_SYM_MUTEX           "__dl__ZL10g_dl_mutex"
#define XDL_LINKER_SYM_DLOPEN_EXT_N    "__dl__ZL10dlopen_extPKciPK17android_dlextinfoPv"
#define XDL_LINKER_SYM_DO_DLOPEN_N     "__dl__Z9do_dlopenPKciPK17android_dlextinfoPv"
#define XDL_LINKER_SYM_DLOPEN_O        "__dl__Z8__dlopenPKciPKv"
#define XDL_LINKER_SYM_LOADER_DLOPEN_P "__loader_dlopen"

#ifndef __LP64__
#define LIB "lib"
#else
#define LIB "lib64"
#endif

typedef void *(*xdl_linker_dlopen_n_t)(const char *, int, const void *, void *);
typedef void *(*xdl_linker_dlopen_o_t)(const char *, int, const void *);

static pthread_mutex_t *xdl_linker_mutex = NULL;
static void *xdl_linker_dlopen = NULL;

typedef enum { MATCH_PREFIX, MATCH_SUFFIX } xdl_linker_match_type_t;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct {
  xdl_linker_match_type_t type;
  const char *value;
} xdl_linker_match_t;
#pragma clang diagnostic pop

typedef struct {
  void *addr;
  xdl_linker_match_t *matches;
  size_t matches_cursor;
} xdl_linker_caller_t;

// https://source.android.com/docs/core/architecture/vndk/linker-namespace
// The following rules are loose and incomplete, you can add more according to your needs.
static xdl_linker_match_t xdl_linker_match_default[] = {{MATCH_SUFFIX, "/libc.so"}};
static xdl_linker_match_t xdl_linker_match_art[] = {{MATCH_SUFFIX, "/libart.so"}};
static xdl_linker_match_t xdl_linker_match_sphal[] = {{MATCH_PREFIX, "/vendor/" LIB "/egl/"},
                                                      {MATCH_PREFIX, "/vendor/" LIB "/hw/"},
                                                      {MATCH_PREFIX, "/vendor/" LIB "/"},
                                                      {MATCH_PREFIX, "/odm/" LIB "/"}};
static xdl_linker_match_t xdl_linker_match_vndk[] = {{MATCH_PREFIX, "/apex/com.android.vndk.v"},
                                                     {MATCH_PREFIX, "/vendor/" LIB "/vndk-sp/"},
                                                     {MATCH_PREFIX, "/odm/" LIB "/vndk-sp/"}};
static xdl_linker_caller_t xdl_linker_callers[] = {
    {NULL, xdl_linker_match_default, sizeof(xdl_linker_match_default) / sizeof(xdl_linker_match_t)},
    {NULL, xdl_linker_match_art, sizeof(xdl_linker_match_art) / sizeof(xdl_linker_match_t)},
    {NULL, xdl_linker_match_sphal, sizeof(xdl_linker_match_sphal) / sizeof(xdl_linker_match_t)},
    {NULL, xdl_linker_match_vndk, sizeof(xdl_linker_match_vndk) / sizeof(xdl_linker_match_t)}};

static void xdl_linker_init_symbols_impl(void) {
  // find linker from: /proc/self/maps (API level < 18) or getauxval (API level >= 18)
  void *handle = xdl_open(XDL_UTIL_LINKER_BASENAME, XDL_DEFAULT);
  if (NULL == handle) return;

  int api_level = xdl_util_get_api_level();
  if (__ANDROID_API_L__ == api_level || __ANDROID_API_L_MR1__ == api_level) {
    // == Android 5.x
    xdl_linker_mutex = (pthread_mutex_t *)xdl_dsym(handle, XDL_LINKER_SYM_MUTEX, NULL);
  } else if (__ANDROID_API_N__ == api_level || __ANDROID_API_N_MR1__ == api_level) {
    // == Android 7.x
    xdl_linker_dlopen = xdl_dsym(handle, XDL_LINKER_SYM_DLOPEN_EXT_N, NULL);
    if (NULL == xdl_linker_dlopen) {
      xdl_linker_dlopen = xdl_dsym(handle, XDL_LINKER_SYM_DO_DLOPEN_N, NULL);
      xdl_linker_mutex = (pthread_mutex_t *)xdl_dsym(handle, XDL_LINKER_SYM_MUTEX, NULL);
    }
  } else if (__ANDROID_API_O__ == api_level || __ANDROID_API_O_MR1__ == api_level) {
    // == Android 8.x
    xdl_linker_dlopen = xdl_dsym(handle, XDL_LINKER_SYM_DLOPEN_O, NULL);
  } else if (api_level >= __ANDROID_API_P__) {
    // >= Android 9.0
    xdl_linker_dlopen = xdl_sym(handle, XDL_LINKER_SYM_LOADER_DLOPEN_P, NULL);
  }

  xdl_close(handle);
}

static void xdl_linker_init_symbols(void) {
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
  static bool inited = false;
  if (!inited) {
    pthread_mutex_lock(&lock);
    if (!inited) {
      xdl_linker_init_symbols_impl();
      inited = true;
    }
    pthread_mutex_unlock(&lock);
  }
}

void xdl_linker_lock(void) {
  xdl_linker_init_symbols();

  if (NULL != xdl_linker_mutex) pthread_mutex_lock(xdl_linker_mutex);
}

void xdl_linker_unlock(void) {
  if (NULL != xdl_linker_mutex) pthread_mutex_unlock(xdl_linker_mutex);
}

static void *xdl_linker_get_caller_addr(struct dl_phdr_info *info) {
  for (size_t i = 0; i < info->dlpi_phnum; i++) {
    const ElfW(Phdr) *phdr = &(info->dlpi_phdr[i]);
    if (PT_LOAD == phdr->p_type) {
      return (void *)(info->dlpi_addr + phdr->p_vaddr);
    }
  }
  return NULL;
}

static void xdl_linker_save_caller_addr(struct dl_phdr_info *info, xdl_linker_caller_t *caller,
                                        size_t cursor) {
  void *addr = xdl_linker_get_caller_addr(info);
  if (NULL != addr) {
    caller->addr = addr;
    caller->matches_cursor = cursor;
  }
}

static int xdl_linker_get_caller_addr_cb(struct dl_phdr_info *info, size_t size, void *arg) {
  (void)size, (void)arg;
  if (0 == info->dlpi_addr || NULL == info->dlpi_name) return 0;  // continue

  int ret = 1;  // OK
  for (size_t i = 0; i < sizeof(xdl_linker_callers) / sizeof(xdl_linker_callers[0]); i++) {
    xdl_linker_caller_t *caller = &xdl_linker_callers[i];
    for (size_t j = 0; j < caller->matches_cursor; j++) {
      xdl_linker_match_t *match = &caller->matches[j];
      switch (match->type) {
        case MATCH_PREFIX:
          if (xdl_util_starts_with(info->dlpi_name, match->value)) {
            xdl_linker_save_caller_addr(info, caller, j);
          }
          break;
        case MATCH_SUFFIX:
          if (xdl_util_ends_with(info->dlpi_name, match->value)) {
            xdl_linker_save_caller_addr(info, caller, j);
          }
          break;
      }
    }
    if (NULL == caller->addr || 0 != caller->matches_cursor) ret = 0;  // continue
  }
  return ret;
}

static void xdl_linker_init_caller_addr_impl(void) {
  xdl_iterate_phdr_impl(xdl_linker_get_caller_addr_cb, NULL, XDL_DEFAULT);
}

static void xdl_linker_init_caller_addr(void) {
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
  static bool inited = false;
  if (!inited) {
    pthread_mutex_lock(&lock);
    if (!inited) {
      xdl_linker_init_caller_addr_impl();
      inited = true;
    }
    pthread_mutex_unlock(&lock);
  }
}

void *xdl_linker_force_dlopen(const char *filename) {
  int api_level = xdl_util_get_api_level();

  if (api_level <= __ANDROID_API_M__) {
    // <= Android 6.0
    return dlopen(filename, RTLD_NOW);
  } else {
    xdl_linker_init_symbols();
    if (NULL == xdl_linker_dlopen) return NULL;
    xdl_linker_init_caller_addr();

    void *handle = NULL;
    if (__ANDROID_API_N__ == api_level || __ANDROID_API_N_MR1__ == api_level) {
      // == Android 7.x
      xdl_linker_lock();
      for (size_t i = 0; i < sizeof(xdl_linker_callers) / sizeof(xdl_linker_callers[0]); i++) {
        xdl_linker_caller_t *caller = &xdl_linker_callers[i];
        if (NULL != caller->addr) {
          handle = ((xdl_linker_dlopen_n_t)xdl_linker_dlopen)(filename, RTLD_NOW, NULL, caller->addr);
          if (NULL != handle) break;
        }
      }
      xdl_linker_unlock();
    } else {
      // >= Android 8.0
      for (size_t i = 0; i < sizeof(xdl_linker_callers) / sizeof(xdl_linker_callers[0]); i++) {
        xdl_linker_caller_t *caller = &xdl_linker_callers[i];
        if (NULL != caller->addr) {
          handle = ((xdl_linker_dlopen_o_t)xdl_linker_dlopen)(filename, RTLD_NOW, caller->addr);
          if (NULL != handle) break;
        }
      }
    }
    return handle;
  }
}
