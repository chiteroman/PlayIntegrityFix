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

#pragma once
#include <android/api-level.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#if defined(__LP64__)
#define BH_UTIL_PRIxADDR "016" PRIxPTR
#else
#define BH_UTIL_PRIxADDR "08" PRIxPTR
#endif

#define BH_UTIL_TEMP_FAILURE_RETRY(exp)    \
  ({                                       \
    __typeof__(exp) _rc;                   \
    do {                                   \
      errno = 0;                           \
      _rc = (exp);                         \
    } while (_rc == -1 && errno == EINTR); \
    _rc;                                   \
  })

int bh_util_set_addr_protect(void *addr, int prot);
int bh_util_set_protect(void *start, void *end, int prot);

bool bh_util_starts_with(const char *str, const char *start);
bool bh_util_ends_with(const char *str, const char *ending);

size_t bh_util_trim_ending(char *start);

int bh_util_get_api_level(void);

int bh_util_write(int fd, const char *buf, size_t buf_len);

struct tm *bh_util_localtime_r(const time_t *timep, long gmtoff, struct tm *result);

size_t bh_util_vsnprintf(char *buffer, size_t buffer_size, const char *format, va_list args);
size_t bh_util_snprintf(char *buffer, size_t buffer_size, const char *format, ...);
