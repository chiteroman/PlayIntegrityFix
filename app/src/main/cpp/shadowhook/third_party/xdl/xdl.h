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

// Created by caikelun on 2020-10-04.

//
// xDL version: 2.2.0
//
// xDL is an enhanced implementation of the Android DL series functions.
// For more information, documentation, and the latest version please check:
// https://github.com/hexhacking/xDL
//

#ifndef IO_GITHUB_HEXHACKING_XDL
#define IO_GITHUB_HEXHACKING_XDL

#include <dlfcn.h>
#include <link.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  // same as Dl_info:
  const char *dli_fname;  // Pathname of shared object that contains address.
  void *dli_fbase;        // Address at which shared object is loaded.
  const char *dli_sname;  // Name of nearest symbol with address lower than addr.
  void *dli_saddr;        // Exact address of symbol named in dli_sname.
  // added by xDL:
  size_t dli_ssize;             // Symbol size of nearest symbol with address lower than addr.
  const ElfW(Phdr) *dlpi_phdr;  // Pointer to array of ELF program headers for this object.
  size_t dlpi_phnum;            // Number of items in dlpi_phdr.
} xdl_info_t;

//
// Default value for flags in xdl_open(), xdl_addr4(), and xdl_iterate_phdr().
//
#define XDL_DEFAULT 0x00

//
// Enhanced dlopen() / dlclose() / dlsym().
//
#define XDL_TRY_FORCE_LOAD    0x01
#define XDL_ALWAYS_FORCE_LOAD 0x02
void *xdl_open(const char *filename, int flags);
void *xdl_close(void *handle);
void *xdl_sym(void *handle, const char *symbol, size_t *symbol_size);
void *xdl_dsym(void *handle, const char *symbol, size_t *symbol_size);

//
// Enhanced dladdr().
//
#define XDL_NON_SYM 0x01
int xdl_addr(void *addr, xdl_info_t *info, void **cache);
int xdl_addr4(void *addr, xdl_info_t *info, void **cache, int flags);
void xdl_addr_clean(void **cache);

//
// Enhanced dl_iterate_phdr().
//
#define XDL_FULL_PATHNAME 0x01
int xdl_iterate_phdr(int (*callback)(struct dl_phdr_info *, size_t, void *), void *data, int flags);

//
// Custom dlinfo().
//
#define XDL_DI_DLINFO 1  // type of info: xdl_info_t
int xdl_info(void *handle, int request, void *info);

#ifdef __cplusplus
}
#endif

#endif
