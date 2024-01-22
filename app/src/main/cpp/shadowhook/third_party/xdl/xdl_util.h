// Copyright (c) 2020-2023 HexHacking Team
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

#ifndef IO_GITHUB_HEXHACKING_XDL_UTIL
#define IO_GITHUB_HEXHACKING_XDL_UTIL

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef __LP64__
#define XDL_UTIL_LINKER_BASENAME        "linker"
#define XDL_UTIL_LINKER_PATHNAME        "/system/bin/linker"
#define XDL_UTIL_APP_PROCESS_BASENAME   "app_process32"
#define XDL_UTIL_APP_PROCESS_PATHNAME   "/system/bin/app_process32"
#define XDL_UTIL_APP_PROCESS_BASENAME_K "app_process"
#define XDL_UTIL_APP_PROCESS_PATHNAME_K "/system/bin/app_process"
#else
#define XDL_UTIL_LINKER_BASENAME      "linker64"
#define XDL_UTIL_LINKER_PATHNAME      "/system/bin/linker64"
#define XDL_UTIL_APP_PROCESS_BASENAME "app_process64"
#define XDL_UTIL_APP_PROCESS_PATHNAME "/system/bin/app_process64"
#endif
#define XDL_UTIL_VDSO_BASENAME "[vdso]"

#define XDL_UTIL_TEMP_FAILURE_RETRY(exp)   \
  ({                                       \
    __typeof__(exp) _rc;                   \
    do {                                   \
      errno = 0;                           \
      _rc = (exp);                         \
    } while (_rc == -1 && errno == EINTR); \
    _rc;                                   \
  })

#ifdef __cplusplus
extern "C" {
#endif

bool xdl_util_starts_with(const char *str, const char *start);
bool xdl_util_ends_with(const char *str, const char *ending);

size_t xdl_util_trim_ending(char *start);

int xdl_util_get_api_level(void);

#ifdef __cplusplus
}
#endif

#endif
