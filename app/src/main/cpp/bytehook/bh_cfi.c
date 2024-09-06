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

// Created by Li Han (hanli.lee@bytedance.com) on 2020-11-04.

#include "bh_cfi.h"

#if defined(__aarch64__)

#include <dlfcn.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "bh_util.h"
#include "bytesig.h"

#define BH_CFI_LIB_DL         "libdl.so"
#define BH_CFI_SLOWPATH       "__cfi_slowpath"
#define BH_CFI_SLOWPATH_DIAG  "__cfi_slowpath_diag"
#define BH_CFI_ARM64_RET_INST 0xd65f03c0

static void *bh_cfi_slowpath = NULL;
static void *bh_cfi_slowpath_diag = NULL;

__attribute__((constructor)) static void bh_cfi_ctor(void) {
  void *handle = dlopen(BH_CFI_LIB_DL, RTLD_NOW);
  if (NULL != handle) {
    bh_cfi_slowpath = dlsym(handle, BH_CFI_SLOWPATH);
    bh_cfi_slowpath_diag = dlsym(handle, BH_CFI_SLOWPATH_DIAG);
    dlclose(handle);
  }
}

int bh_cfi_disable_slowpath(void) {
  if (bh_util_get_api_level() < __ANDROID_API_O__) return 0;

  if (NULL == bh_cfi_slowpath || NULL == bh_cfi_slowpath_diag) return -1;

  void *start = bh_cfi_slowpath <= bh_cfi_slowpath_diag ? bh_cfi_slowpath : bh_cfi_slowpath_diag;
  void *end = bh_cfi_slowpath <= bh_cfi_slowpath_diag ? bh_cfi_slowpath_diag : bh_cfi_slowpath;
  if (0 != bh_util_set_protect(start, (void *)((uintptr_t)end + sizeof(uint32_t)),
                               PROT_READ | PROT_WRITE | PROT_EXEC))
    return -1;

  BYTESIG_TRY(SIGSEGV, SIGBUS) {
    *((uint32_t *)bh_cfi_slowpath) = BH_CFI_ARM64_RET_INST;
    *((uint32_t *)bh_cfi_slowpath_diag) = BH_CFI_ARM64_RET_INST;
  }
  BYTESIG_CATCH() {
    return -1;
  }
  BYTESIG_EXIT

  __builtin___clear_cache(start, (void *)((size_t)end + sizeof(uint32_t)));

  return 0;
}

#else

int bh_cfi_disable_slowpath(void) {
  return 0;
}

#endif
