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

// Created by Kelun Cai (caikelun@bytedance.com) on 2021-10-18.

#include "bh_recorder.h"

#include <dlfcn.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "bh_util.h"
#include "bytehook.h"

#define BH_RECORDER_OP_HOOK   0
#define BH_RECORDER_OP_UNHOOK 1

#define BH_RECORDER_LIB_NAME_MAX 512
#define BH_RECORDER_SYM_NAME_MAX 1024

#define BH_RECORDER_STRINGS_BUF_EXPAND_STEP (1024 * 16)
#define BH_RECORDER_STRINGS_BUF_MAX         (1024 * 128)
#define BH_RECORDER_RECORDS_BUF_EXPAND_STEP (1024 * 32)
#define BH_RECORDER_RECORDS_BUF_MAX         (1024 * 384)
#define BH_RECORDER_OUTPUT_BUF_EXPAND_STEP  (1024 * 128)
#define BH_RECORDER_OUTPUT_BUF_MAX          (1024 * 1024)

static bool bh_recorder_recordable = false;

bool bh_recorder_get_recordable(void) {
  return bh_recorder_recordable;
}

void bh_recorder_set_recordable(bool recordable) {
  bh_recorder_recordable = recordable;
}

typedef struct {
  void *ptr;
  size_t cap;
  size_t sz;
  pthread_mutex_t lock;
} bh_recorder_buf_t;

static int bh_recorder_buf_append(bh_recorder_buf_t *buf, size_t step, size_t max, const void *header,
                                  size_t header_sz, const void *body, size_t body_sz) {
  size_t needs = (header_sz + (NULL != body ? body_sz : 0));
  if (needs > step) return -1;

  if (buf->cap - buf->sz < needs) {
    size_t new_cap = buf->cap + step;
    if (new_cap > max) return -1;
    void *new_ptr = realloc(buf->ptr, new_cap);
    if (NULL == new_ptr) return -1;
    buf->ptr = new_ptr;
    buf->cap = new_cap;
  }

  memcpy((void *)((uintptr_t)buf->ptr + buf->sz), header, header_sz);
  if (NULL != body) memcpy((void *)((uintptr_t)buf->ptr + buf->sz + header_sz), body, body_sz);
  buf->sz += needs;
  return 0;
}

static void bh_recorder_buf_free(bh_recorder_buf_t *buf) {
  if (NULL != buf->ptr) {
    free(buf->ptr);
    buf->ptr = NULL;
  }
}

static bh_recorder_buf_t bh_recorder_strings = {NULL, 0, 0, PTHREAD_MUTEX_INITIALIZER};
static bh_recorder_buf_t bh_recorder_records = {NULL, 0, 0, PTHREAD_MUTEX_INITIALIZER};
static bool bh_recorder_error = false;

typedef struct {
  uint16_t str_len;  // body length, in order to speed up the search
} __attribute__((packed)) bh_recorder_str_header_t;
// +body: string, including the terminating null byte ('\0')

typedef struct {
  uint64_t op : 8;
  uint64_t error_number : 8;
  uint64_t ts_ms : 48;
  uintptr_t stub;
  uint16_t caller_lib_name_idx;
  uint16_t lib_name_idx;
  uint16_t sym_name_idx;
  uintptr_t new_addr;
} __attribute__((packed)) bh_recorder_record_hook_header_t;
// no body

typedef struct {
  uint64_t op : 8;
  uint64_t error_number : 8;
  uint64_t ts_ms : 48;
  uintptr_t stub;
  uint16_t caller_lib_name_idx;
} __attribute__((packed)) bh_recorder_record_unhook_header_t;
// no body

static int bh_recorder_add_str(const char *str, size_t str_len, uint16_t *str_idx) {
  uint16_t idx = 0;
  bool ok = false;

  pthread_mutex_lock(&bh_recorder_strings.lock);

  // find in existing strings
  size_t i = 0;
  while (i < bh_recorder_strings.sz) {
    bh_recorder_str_header_t *header = (bh_recorder_str_header_t *)((uintptr_t)bh_recorder_strings.ptr + i);
    if (header->str_len == str_len) {
      void *tmp = (void *)((uintptr_t)bh_recorder_strings.ptr + i + sizeof(header->str_len));
      if (0 == memcmp(tmp, str, str_len)) {
        *str_idx = idx;
        ok = true;
        break;  // OK
      }
    }
    i += (sizeof(bh_recorder_str_header_t) + header->str_len + 1);
    idx++;
    if (idx == UINT16_MAX) break;  // failed
  }

  // insert a new string
  if (!ok && idx < UINT16_MAX) {
    // append new string
    bh_recorder_str_header_t header = {(uint16_t)str_len};
    if (0 == bh_recorder_buf_append(&bh_recorder_strings, BH_RECORDER_STRINGS_BUF_EXPAND_STEP,
                                    BH_RECORDER_STRINGS_BUF_MAX, &header, sizeof(header), str, str_len + 1)) {
      *str_idx = idx;
      ok = true;  // OK
    }
  }

  pthread_mutex_unlock(&bh_recorder_strings.lock);

  return ok ? 0 : -1;
}

static char *bh_recorder_find_str(uint16_t idx) {
  uint16_t cur_idx = 0;

  size_t i = 0;
  while (i < bh_recorder_strings.sz && cur_idx < idx) {
    bh_recorder_str_header_t *header = (bh_recorder_str_header_t *)((uintptr_t)bh_recorder_strings.ptr + i);
    i += (sizeof(bh_recorder_str_header_t) + header->str_len + 1);
    cur_idx++;
  }
  if (cur_idx != idx) return "error";

  bh_recorder_str_header_t *header = (bh_recorder_str_header_t *)((uintptr_t)bh_recorder_strings.ptr + i);
  return (char *)((uintptr_t)header + sizeof(bh_recorder_str_header_t));
}

static long bh_recorder_tz = LONG_MAX;

static uint64_t bh_recorder_get_timestamp_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  if (LONG_MAX == bh_recorder_tz) {
    //    struct tm tm;
    //    if (NULL != localtime_r((time_t *)(&(tv.tv_sec)), &tm)) bh_recorder_tz = tm.tm_gmtoff;
    bh_recorder_tz = 0;
  }

  return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

static size_t bh_recorder_format_timestamp_ms(uint64_t ts_ms, char *buf, size_t buf_len) {
  time_t sec = (time_t)(ts_ms / 1000);
  suseconds_t msec = (time_t)(ts_ms % 1000);

  struct tm tm;
  bh_util_localtime_r(&sec, bh_recorder_tz, &tm);

  return bh_util_snprintf(buf, buf_len, "%04d-%02d-%02dT%02d:%02d:%02d.%03ld%c%02ld:%02ld,",
                          tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
                          msec, bh_recorder_tz < 0 ? '-' : '+', labs(bh_recorder_tz / 3600),
                          labs(bh_recorder_tz % 3600));
}

static const char *bh_recorder_get_basename(const char *lib_name) {
  const char *p = strrchr(lib_name, '/');
  if (NULL != p && '\0' != *(p + 1))
    return p + 1;
  else
    return lib_name;
}

static void bh_recorder_get_basename_by_addr(uintptr_t addr, char *lib_name, size_t lib_name_sz) {
  Dl_info info;
  if (0 == dladdr((void *)addr, &info) || NULL == info.dli_fname || '\0' == info.dli_fname[0])
    strlcpy(lib_name, "unknown", lib_name_sz);
  else {
    const char *p = strrchr(info.dli_fname, '/');
    if (NULL == p || '\0' == *(p + 1))
      p = info.dli_fname;
    else
      p++;
    strlcpy(lib_name, p, lib_name_sz);
  }
}

int bh_recorder_add_hook(int error_number, const char *lib_name, const char *sym_name, uintptr_t new_addr,
                         uintptr_t stub, uintptr_t caller_addr) {
  if (!bh_recorder_recordable) return 0;
  if (bh_recorder_error) return -1;

  // lib_name
  if (NULL == lib_name)
    lib_name = "unknown";
  else
    lib_name = bh_recorder_get_basename(lib_name);
  size_t lib_name_len = strlen(lib_name);
  if (0 == lib_name_len || lib_name_len > BH_RECORDER_LIB_NAME_MAX) return -1;

  // sym_name
  if (NULL == sym_name) return -1;
  size_t sym_name_len = strlen(sym_name);
  if (0 == sym_name_len || sym_name_len > BH_RECORDER_SYM_NAME_MAX) return -1;

  // caller_lib_name
  char caller_lib_name[BH_RECORDER_LIB_NAME_MAX];
  bh_recorder_get_basename_by_addr(caller_addr, caller_lib_name, sizeof(caller_lib_name));
  size_t caller_lib_name_len = strlen(caller_lib_name);

  // add strings to strings-pool
  uint16_t lib_name_idx, sym_name_idx, caller_lib_name_idx;
  if (0 != bh_recorder_add_str(lib_name, lib_name_len, &lib_name_idx)) goto err;
  if (0 != bh_recorder_add_str(sym_name, sym_name_len, &sym_name_idx)) goto err;
  if (0 != bh_recorder_add_str(caller_lib_name, caller_lib_name_len, &caller_lib_name_idx)) goto err;

  // append new hook record
  bh_recorder_record_hook_header_t header = {BH_RECORDER_OP_HOOK,
                                             (uint8_t)error_number,
                                             bh_recorder_get_timestamp_ms(),
                                             stub,
                                             caller_lib_name_idx,
                                             lib_name_idx,
                                             sym_name_idx,
                                             new_addr};
  pthread_mutex_lock(&bh_recorder_records.lock);
  int r = bh_recorder_buf_append(&bh_recorder_records, BH_RECORDER_RECORDS_BUF_EXPAND_STEP,
                                 BH_RECORDER_RECORDS_BUF_MAX, &header, sizeof(header), NULL, 0);
  pthread_mutex_unlock(&bh_recorder_records.lock);
  if (0 != r) goto err;

  return 0;

err:
  bh_recorder_error = true;
  return -1;
}

int bh_recorder_add_unhook(int error_number, uintptr_t stub, uintptr_t caller_addr) {
  if (!bh_recorder_recordable) return 0;
  if (bh_recorder_error) return -1;

  char caller_lib_name[BH_RECORDER_LIB_NAME_MAX];
  bh_recorder_get_basename_by_addr(caller_addr, caller_lib_name, sizeof(caller_lib_name));
  size_t caller_lib_name_len = strlen(caller_lib_name);

  uint16_t caller_lib_name_idx;
  if (0 != bh_recorder_add_str(caller_lib_name, caller_lib_name_len, &caller_lib_name_idx)) goto err;

  bh_recorder_record_unhook_header_t header = {BH_RECORDER_OP_UNHOOK, (uint8_t)error_number,
                                               bh_recorder_get_timestamp_ms(), stub, caller_lib_name_idx};
  pthread_mutex_lock(&bh_recorder_records.lock);
  int r = bh_recorder_buf_append(&bh_recorder_records, BH_RECORDER_RECORDS_BUF_EXPAND_STEP,
                                 BH_RECORDER_RECORDS_BUF_MAX, &header, sizeof(header), NULL, 0);
  pthread_mutex_unlock(&bh_recorder_records.lock);
  if (0 != r) goto err;

  return 0;

err:
  bh_recorder_error = true;
  return -1;
}

static const char *bh_recorder_get_op_name(uint8_t op) {
  switch (op) {
    case BH_RECORDER_OP_HOOK:
      return "hook";
    case BH_RECORDER_OP_UNHOOK:
      return "unhook";
    default:
      return "error";
  }
}

static void bh_recorder_output(char **str, int fd, uint32_t item_flags) {
  if (NULL == bh_recorder_records.ptr || 0 == bh_recorder_records.sz) return;

  bh_recorder_buf_t output = {NULL, 0, 0, PTHREAD_MUTEX_INITIALIZER};

  pthread_mutex_lock(&bh_recorder_records.lock);
  pthread_mutex_lock(&bh_recorder_strings.lock);

  char line[BH_RECORDER_LIB_NAME_MAX * 2 + BH_RECORDER_SYM_NAME_MAX + 256];
  size_t line_sz;
  size_t i = 0;
  while (i < bh_recorder_records.sz) {
    line_sz = 0;
    bh_recorder_record_hook_header_t *header =
        (bh_recorder_record_hook_header_t *)((uintptr_t)bh_recorder_records.ptr + i);

    if (item_flags & BYTEHOOK_RECORD_ITEM_TIMESTAMP)
      line_sz += bh_recorder_format_timestamp_ms(header->ts_ms, line + line_sz, sizeof(line) - line_sz);
    if (item_flags & BYTEHOOK_RECORD_ITEM_CALLER_LIB_NAME)
      line_sz += bh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%s,",
                                  bh_recorder_find_str(header->caller_lib_name_idx));
    if (item_flags & BYTEHOOK_RECORD_ITEM_OP)
      line_sz += bh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%s,",
                                  bh_recorder_get_op_name(header->op));
    if ((item_flags & BYTEHOOK_RECORD_ITEM_LIB_NAME) && header->op != BH_RECORDER_OP_UNHOOK)
      line_sz += bh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%s,",
                                  bh_recorder_find_str(header->lib_name_idx));
    if ((item_flags & BYTEHOOK_RECORD_ITEM_SYM_NAME) && header->op != BH_RECORDER_OP_UNHOOK)
      line_sz += bh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%s,",
                                  bh_recorder_find_str(header->sym_name_idx));
    if ((item_flags & BYTEHOOK_RECORD_ITEM_NEW_ADDR) && header->op != BH_RECORDER_OP_UNHOOK)
      line_sz += bh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%" PRIxPTR ",", header->new_addr);
    if (item_flags & BYTEHOOK_RECORD_ITEM_ERRNO)
      line_sz +=
          bh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%" PRIu8 ",", header->error_number);
    if (item_flags & BYTEHOOK_RECORD_ITEM_STUB)
      line_sz += bh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%" PRIxPTR ",", header->stub);
    line[line_sz - 1] = '\n';

    if (NULL != str) {
      // append to string
      if (0 != bh_recorder_buf_append(&output, BH_RECORDER_OUTPUT_BUF_EXPAND_STEP, BH_RECORDER_OUTPUT_BUF_MAX,
                                      line, line_sz, NULL, 0)) {
        bh_recorder_buf_free(&output);
        break;  // failed
      }
    } else {
      // write to FD
      if (0 != bh_util_write(fd, line, line_sz)) break;  // failed
    }

    i += (BH_RECORDER_OP_UNHOOK == header->op ? sizeof(bh_recorder_record_unhook_header_t)
                                              : sizeof(bh_recorder_record_hook_header_t));
  }

  pthread_mutex_unlock(&bh_recorder_strings.lock);
  pthread_mutex_unlock(&bh_recorder_records.lock);

  // error message
  if (bh_recorder_error) {
    line_sz = 0;

    if (item_flags & BYTEHOOK_RECORD_ITEM_TIMESTAMP)
      line_sz += bh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "9999-99-99T00:00:00.000+00:00,");
    if (item_flags & BYTEHOOK_RECORD_ITEM_CALLER_LIB_NAME)
      line_sz += bh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "error,");
    if (item_flags & BYTEHOOK_RECORD_ITEM_OP)
      line_sz += bh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "error,");

    if (0 == line_sz) line_sz = bh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "error,");

    line[line_sz - 1] = '\n';

    if (NULL != str) {
      // append to string
      if (0 != bh_recorder_buf_append(&output, BH_RECORDER_OUTPUT_BUF_EXPAND_STEP, BH_RECORDER_OUTPUT_BUF_MAX,
                                      line, line_sz, NULL, 0)) {
        bh_recorder_buf_free(&output);
        return;  // failed
      }
    } else {
      // write to FD
      if (0 != bh_util_write(fd, line, line_sz)) return;  // failed
    }
  }

  // return string
  if (NULL != str) {
    if (0 != bh_recorder_buf_append(&output, BH_RECORDER_OUTPUT_BUF_EXPAND_STEP, BH_RECORDER_OUTPUT_BUF_MAX,
                                    "", 1, NULL, 0)) {
      bh_recorder_buf_free(&output);
      return;  // failed
    }
    *str = output.ptr;
  }
}

char *bh_recorder_get(uint32_t item_flags) {
  if (!bh_recorder_recordable) return NULL;
  if (0 == (item_flags & BYTEHOOK_RECORD_ITEM_ALL)) return NULL;

  char *str = NULL;
  bh_recorder_output(&str, -1, item_flags);
  return str;
}

void bh_recorder_dump(int fd, uint32_t item_flags) {
  if (!bh_recorder_recordable) return;
  if (0 == (item_flags & BYTEHOOK_RECORD_ITEM_ALL)) return;
  if (fd < 0) return;
  bh_recorder_output(NULL, fd, item_flags);
}
