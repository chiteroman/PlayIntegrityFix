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

// Created by Kelun Cai (caikelun@bytedance.com) on 2021-04-11.

#include "sh_recorder.h"

#include "sh_config.h"

#ifdef SH_CONFIG_OPERATION_RECORDS

#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "sh_sig.h"
#include "sh_util.h"
#include "shadowhook.h"
#include "xdl.h"

#define SH_RECORDER_OP_HOOK_SYM_ADDR 0
#define SH_RECORDER_OP_HOOK_SYM_NAME 1
#define SH_RECORDER_OP_UNHOOK        2

#define SH_RECORDER_LIB_NAME_MAX 512
#define SH_RECORDER_SYM_NAME_MAX 1024

#define SH_RECORDER_STRINGS_BUF_EXPAND_STEP (1024 * 16)
#define SH_RECORDER_STRINGS_BUF_MAX         (1024 * 128)
#define SH_RECORDER_RECORDS_BUF_EXPAND_STEP (1024 * 32)
#define SH_RECORDER_RECORDS_BUF_MAX         (1024 * 384)
#define SH_RECORDER_OUTPUT_BUF_EXPAND_STEP  (1024 * 128)
#define SH_RECORDER_OUTPUT_BUF_MAX          (1024 * 1024)

static bool sh_recorder_recordable = false;

bool sh_recorder_get_recordable(void) {
  return sh_recorder_recordable;
}

void sh_recorder_set_recordable(bool recordable) {
  sh_recorder_recordable = recordable;
}

typedef struct {
  void *ptr;
  size_t cap;
  size_t sz;
  pthread_mutex_t lock;
} sh_recorder_buf_t;

static int sh_recorder_buf_append(sh_recorder_buf_t *buf, size_t step, size_t max, const void *header,
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

static void sh_recorder_buf_free(sh_recorder_buf_t *buf) {
  if (NULL != buf->ptr) {
    free(buf->ptr);
    buf->ptr = NULL;
  }
}

static sh_recorder_buf_t sh_recorder_strings = {NULL, 0, 0, PTHREAD_MUTEX_INITIALIZER};
static sh_recorder_buf_t sh_recorder_records = {NULL, 0, 0, PTHREAD_MUTEX_INITIALIZER};
static bool sh_recorder_error = false;

typedef struct {
  uint16_t str_len;  // body length, in order to speed up the search
} __attribute__((packed)) sh_recorder_str_header_t;
// +body: string, including the terminating null byte ('\0')

typedef struct {
  uint64_t op : 8;
  uint64_t error_number : 8;
  uint64_t ts_ms : 48;
  uintptr_t stub;
  uint16_t caller_lib_name_idx;
  uint8_t backup_len;
  uint16_t lib_name_idx;
  uint16_t sym_name_idx;
  uintptr_t sym_addr;
  uintptr_t new_addr;
} __attribute__((packed)) sh_recorder_record_hook_header_t;
// no body

typedef struct {
  uint64_t op : 8;
  uint64_t error_number : 8;
  uint64_t ts_ms : 48;
  uintptr_t stub;
  uint16_t caller_lib_name_idx;
} __attribute__((packed)) sh_recorder_record_unhook_header_t;
// no body

static int sh_recorder_add_str(const char *str, size_t str_len, uint16_t *str_idx) {
  uint16_t idx = 0;
  bool ok = false;

  pthread_mutex_lock(&sh_recorder_strings.lock);

  // find in existing strings
  size_t i = 0;
  while (i < sh_recorder_strings.sz) {
    sh_recorder_str_header_t *header = (sh_recorder_str_header_t *)((uintptr_t)sh_recorder_strings.ptr + i);
    if (header->str_len == str_len) {
      void *tmp = (void *)((uintptr_t)sh_recorder_strings.ptr + i + sizeof(header->str_len));
      if (0 == memcmp(tmp, str, str_len)) {
        *str_idx = idx;
        ok = true;
        break;  // OK
      }
    }
    i += (sizeof(sh_recorder_str_header_t) + header->str_len + 1);
    idx++;
    if (idx == UINT16_MAX) break;  // failed
  }

  // insert a new string
  if (!ok && idx < UINT16_MAX) {
    // append new string
    sh_recorder_str_header_t header = {(uint16_t)str_len};
    if (0 == sh_recorder_buf_append(&sh_recorder_strings, SH_RECORDER_STRINGS_BUF_EXPAND_STEP,
                                    SH_RECORDER_STRINGS_BUF_MAX, &header, sizeof(header), str, str_len + 1)) {
      *str_idx = idx;
      ok = true;  // OK
    }
  }

  pthread_mutex_unlock(&sh_recorder_strings.lock);

  return ok ? 0 : -1;
}

static char *sh_recorder_find_str(uint16_t idx) {
  uint16_t cur_idx = 0;

  size_t i = 0;
  while (i < sh_recorder_strings.sz && cur_idx < idx) {
    sh_recorder_str_header_t *header = (sh_recorder_str_header_t *)((uintptr_t)sh_recorder_strings.ptr + i);
    i += (sizeof(sh_recorder_str_header_t) + header->str_len + 1);
    cur_idx++;
  }
  if (cur_idx != idx) return "error";

  sh_recorder_str_header_t *header = (sh_recorder_str_header_t *)((uintptr_t)sh_recorder_strings.ptr + i);
  return (char *)((uintptr_t)header + sizeof(sh_recorder_str_header_t));
}

static long sh_recorder_tz = LONG_MAX;

static uint64_t sh_recorder_get_timestamp_ms(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  if (LONG_MAX == sh_recorder_tz) {
    // localtime_r() will call getenv() without lock protection,
    // and will crash when encountering concurrent setenv() calls.
    // We really encountered.
    sh_recorder_tz = 0;
    //    struct tm tm;
    //    if (NULL != localtime_r((time_t *)(&(tv.tv_sec)), &tm)) sh_recorder_tz = tm.tm_gmtoff;
  }

  return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

static size_t sh_recorder_format_timestamp_ms(uint64_t ts_ms, char *buf, size_t buf_len) {
  time_t sec = (time_t)(ts_ms / 1000);
  time_t msec = (time_t)(ts_ms % 1000);

  struct tm tm;
  sh_util_localtime_r(&sec, sh_recorder_tz, &tm);

  return sh_util_snprintf(buf, buf_len, "%04d-%02d-%02dT%02d:%02d:%02d.%03ld%c%02ld:%02ld,",
                          tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
                          msec, sh_recorder_tz < 0 ? '-' : '+', labs(sh_recorder_tz / 3600),
                          labs(sh_recorder_tz % 3600));
}

static const char *sh_recorder_get_base_name(const char *lib_name) {
  const char *p = strrchr(lib_name, '/');
  if (NULL != p && '\0' != *(p + 1))
    return p + 1;
  else
    return lib_name;
}

static int sh_recorder_get_base_name_by_addr_iterator(struct dl_phdr_info *info, size_t size, void *arg) {
  (void)size;

  uintptr_t *pkg = (uintptr_t *)arg;
  uintptr_t addr = *pkg++;
  char *base_name = (char *)*pkg++;
  size_t base_name_sz = (size_t)*pkg;

  for (size_t i = 0; i < info->dlpi_phnum; i++) {
    const ElfW(Phdr) *phdr = &(info->dlpi_phdr[i]);
    if (PT_LOAD != phdr->p_type) continue;
    if (addr < (uintptr_t)(info->dlpi_addr + phdr->p_vaddr) ||
        addr >= (uintptr_t)(info->dlpi_addr + phdr->p_vaddr + phdr->p_memsz))
      continue;

    // get lib_name from path_name
    const char *p;
    if (NULL == info->dlpi_name || '\0' == info->dlpi_name[0])
      p = "unknown";
    else {
      p = strrchr(info->dlpi_name, '/');
      if (NULL == p || '\0' == *(p + 1))
        p = info->dlpi_name;
      else
        p++;
    }

    strlcpy(base_name, p, base_name_sz);
    return 1;  // OK
  }

  return 0;  // continue
}

static void sh_recorder_get_base_name_by_addr(uintptr_t addr, char *base_name, size_t base_name_sz) {
  base_name[0] = '\0';
  uintptr_t pkg[3] = {addr, (uintptr_t)base_name, (uintptr_t)base_name_sz};

  if (sh_util_get_api_level() >= __ANDROID_API_L__) {
    xdl_iterate_phdr(sh_recorder_get_base_name_by_addr_iterator, pkg, XDL_DEFAULT);
  } else {
    SH_SIG_TRY(SIGSEGV, SIGBUS) {
      xdl_iterate_phdr(sh_recorder_get_base_name_by_addr_iterator, pkg, XDL_DEFAULT);
    }
    SH_SIG_EXIT
  }

  if ('\0' == base_name[0]) strlcpy(base_name, "unknown", base_name_sz);
}

int sh_recorder_add_hook(int error_number, bool is_hook_sym_addr, uintptr_t sym_addr, const char *lib_name,
                         const char *sym_name, uintptr_t new_addr, size_t backup_len, uintptr_t stub,
                         uintptr_t caller_addr) {
  if (!sh_recorder_recordable) return 0;
  if (sh_recorder_error) return -1;

  // lib_name
  if (NULL == lib_name) return -1;
  lib_name = sh_recorder_get_base_name(lib_name);
  size_t lib_name_len = strlen(lib_name);
  if (0 == lib_name_len || lib_name_len > SH_RECORDER_LIB_NAME_MAX) return -1;

  // sym_name
  if (NULL == sym_name) return -1;
  size_t sym_name_len = strlen(sym_name);
  if (0 == sym_name_len || sym_name_len > SH_RECORDER_SYM_NAME_MAX) return -1;

  // caller_lib_name
  char caller_lib_name[SH_RECORDER_LIB_NAME_MAX];
  sh_recorder_get_base_name_by_addr(caller_addr, caller_lib_name, sizeof(caller_lib_name));
  size_t caller_lib_name_len = strlen(caller_lib_name);

  // add strings to strings-pool
  uint16_t lib_name_idx, sym_name_idx, caller_lib_name_idx;
  if (0 != sh_recorder_add_str(lib_name, lib_name_len, &lib_name_idx)) goto err;
  if (0 != sh_recorder_add_str(sym_name, sym_name_len, &sym_name_idx)) goto err;
  if (0 != sh_recorder_add_str(caller_lib_name, caller_lib_name_len, &caller_lib_name_idx)) goto err;

  // append new hook record
  sh_recorder_record_hook_header_t header = {
      is_hook_sym_addr ? SH_RECORDER_OP_HOOK_SYM_ADDR : SH_RECORDER_OP_HOOK_SYM_NAME,
      (uint8_t)error_number,
      sh_recorder_get_timestamp_ms(),
      stub,
      caller_lib_name_idx,
      (uint8_t)backup_len,
      lib_name_idx,
      sym_name_idx,
      sym_addr,
      new_addr};
  pthread_mutex_lock(&sh_recorder_records.lock);
  int r = sh_recorder_buf_append(&sh_recorder_records, SH_RECORDER_RECORDS_BUF_EXPAND_STEP,
                                 SH_RECORDER_RECORDS_BUF_MAX, &header, sizeof(header), NULL, 0);
  pthread_mutex_unlock(&sh_recorder_records.lock);
  if (0 != r) goto err;

  return 0;

err:
  sh_recorder_error = true;
  return -1;
}

int sh_recorder_add_unhook(int error_number, uintptr_t stub, uintptr_t caller_addr) {
  if (!sh_recorder_recordable) return 0;
  if (sh_recorder_error) return -1;

  char caller_lib_name[SH_RECORDER_LIB_NAME_MAX];
  sh_recorder_get_base_name_by_addr(caller_addr, caller_lib_name, sizeof(caller_lib_name));
  size_t caller_lib_name_len = strlen(caller_lib_name);

  uint16_t caller_lib_name_idx;
  if (0 != sh_recorder_add_str(caller_lib_name, caller_lib_name_len, &caller_lib_name_idx)) goto err;

  sh_recorder_record_unhook_header_t header = {SH_RECORDER_OP_UNHOOK, (uint8_t)error_number,
                                               sh_recorder_get_timestamp_ms(), stub, caller_lib_name_idx};
  pthread_mutex_lock(&sh_recorder_records.lock);
  int r = sh_recorder_buf_append(&sh_recorder_records, SH_RECORDER_RECORDS_BUF_EXPAND_STEP,
                                 SH_RECORDER_RECORDS_BUF_MAX, &header, sizeof(header), NULL, 0);
  pthread_mutex_unlock(&sh_recorder_records.lock);
  if (0 != r) goto err;

  return 0;

err:
  sh_recorder_error = true;
  return -1;
}

static const char *sh_recorder_get_op_name(uint8_t op) {
  switch (op) {
    case SH_RECORDER_OP_HOOK_SYM_ADDR:
      return "hook_sym_addr";
    case SH_RECORDER_OP_HOOK_SYM_NAME:
      return "hook_sym_name";
    case SH_RECORDER_OP_UNHOOK:
      return "unhook";
    default:
      return "error";
  }
}

static void sh_recorder_output(char **str, int fd, uint32_t item_flags) {
  if (NULL == sh_recorder_records.ptr || 0 == sh_recorder_records.sz) return;

  sh_recorder_buf_t output = {NULL, 0, 0, PTHREAD_MUTEX_INITIALIZER};

  pthread_mutex_lock(&sh_recorder_records.lock);
  pthread_mutex_lock(&sh_recorder_strings.lock);

  char line[SH_RECORDER_LIB_NAME_MAX * 2 + SH_RECORDER_SYM_NAME_MAX + 256];
  size_t line_sz;
  size_t i = 0;
  while (i < sh_recorder_records.sz) {
    line_sz = 0;
    sh_recorder_record_hook_header_t *header =
        (sh_recorder_record_hook_header_t *)((uintptr_t)sh_recorder_records.ptr + i);

    if (item_flags & SHADOWHOOK_RECORD_ITEM_TIMESTAMP)
      line_sz += sh_recorder_format_timestamp_ms(header->ts_ms, line + line_sz, sizeof(line) - line_sz);
    if (item_flags & SHADOWHOOK_RECORD_ITEM_CALLER_LIB_NAME)
      line_sz += sh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%s,",
                                  sh_recorder_find_str(header->caller_lib_name_idx));
    if (item_flags & SHADOWHOOK_RECORD_ITEM_OP)
      line_sz += sh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%s,",
                                  sh_recorder_get_op_name(header->op));
    if ((item_flags & SHADOWHOOK_RECORD_ITEM_LIB_NAME) && header->op != SH_RECORDER_OP_UNHOOK)
      line_sz += sh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%s,",
                                  sh_recorder_find_str(header->lib_name_idx));
    if ((item_flags & SHADOWHOOK_RECORD_ITEM_SYM_NAME) && header->op != SH_RECORDER_OP_UNHOOK)
      line_sz += sh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%s,",
                                  sh_recorder_find_str(header->sym_name_idx));
    if ((item_flags & SHADOWHOOK_RECORD_ITEM_SYM_ADDR) && header->op != SH_RECORDER_OP_UNHOOK)
      line_sz += sh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%" PRIxPTR ",", header->sym_addr);
    if ((item_flags & SHADOWHOOK_RECORD_ITEM_NEW_ADDR) && header->op != SH_RECORDER_OP_UNHOOK)
      line_sz += sh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%" PRIxPTR ",", header->new_addr);
    if ((item_flags & SHADOWHOOK_RECORD_ITEM_BACKUP_LEN) && header->op != SH_RECORDER_OP_UNHOOK)
      line_sz += sh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%" PRIu8 ",", header->backup_len);
    if (item_flags & SHADOWHOOK_RECORD_ITEM_ERRNO)
      line_sz +=
          sh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%" PRIu8 ",", header->error_number);
    if (item_flags & SHADOWHOOK_RECORD_ITEM_STUB)
      line_sz += sh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "%" PRIxPTR ",", header->stub);
    line[line_sz - 1] = '\n';

    if (NULL != str) {
      // append to string
      if (0 != sh_recorder_buf_append(&output, SH_RECORDER_OUTPUT_BUF_EXPAND_STEP, SH_RECORDER_OUTPUT_BUF_MAX,
                                      line, line_sz, NULL, 0)) {
        sh_recorder_buf_free(&output);
        break;  // failed
      }
    } else {
      // write to FD
      if (0 != sh_util_write(fd, line, line_sz)) break;  // failed
    }

    i += (SH_RECORDER_OP_UNHOOK == header->op ? sizeof(sh_recorder_record_unhook_header_t)
                                              : sizeof(sh_recorder_record_hook_header_t));
  }

  pthread_mutex_unlock(&sh_recorder_strings.lock);
  pthread_mutex_unlock(&sh_recorder_records.lock);

  // error message
  if (sh_recorder_error) {
    line_sz = 0;

    if (item_flags & SHADOWHOOK_RECORD_ITEM_TIMESTAMP)
      line_sz += sh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "9999-99-99T00:00:00.000+00:00,");
    if (item_flags & SHADOWHOOK_RECORD_ITEM_CALLER_LIB_NAME)
      line_sz += sh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "error,");
    if (item_flags & SHADOWHOOK_RECORD_ITEM_OP)
      line_sz += sh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "error,");

    if (0 == line_sz) line_sz = sh_util_snprintf(line + line_sz, sizeof(line) - line_sz, "error,");

    line[line_sz - 1] = '\n';

    if (NULL != str) {
      // append to string
      if (0 != sh_recorder_buf_append(&output, SH_RECORDER_OUTPUT_BUF_EXPAND_STEP, SH_RECORDER_OUTPUT_BUF_MAX,
                                      line, line_sz, NULL, 0)) {
        sh_recorder_buf_free(&output);
        return;  // failed
      }
    } else {
      // write to FD
      if (0 != sh_util_write(fd, line, line_sz)) return;  // failed
    }
  }

  // return string
  if (NULL != str) {
    if (0 != sh_recorder_buf_append(&output, SH_RECORDER_OUTPUT_BUF_EXPAND_STEP, SH_RECORDER_OUTPUT_BUF_MAX,
                                    "", 1, NULL, 0)) {
      sh_recorder_buf_free(&output);
      return;  // failed
    }
    *str = output.ptr;
  }
}

char *sh_recorder_get(uint32_t item_flags) {
  if (!sh_recorder_recordable) return NULL;
  if (0 == (item_flags & SHADOWHOOK_RECORD_ITEM_ALL)) return NULL;

  char *str = NULL;
  sh_recorder_output(&str, -1, item_flags);
  return str;
}

void sh_recorder_dump(int fd, uint32_t item_flags) {
  if (!sh_recorder_recordable) return;
  if (0 == (item_flags & SHADOWHOOK_RECORD_ITEM_ALL)) return;
  if (fd < 0) return;
  sh_recorder_output(NULL, fd, item_flags);
}

#else

bool sh_recorder_get_recordable(void) {
  return false;
}

void sh_recorder_set_recordable(bool recordable) {
  (void)recordable;
}

int sh_recorder_add_hook(int error_number, bool is_hook_sym_addr, uintptr_t sym_addr, const char *lib_name,
                         const char *sym_name, uintptr_t new_addr, size_t backup_len, uintptr_t stub,
                         uintptr_t caller_addr) {
  (void)error_number, (void)is_hook_sym_addr, (void)sym_addr, (void)lib_name, (void)sym_name, (void)new_addr,
      (void)backup_len, (void)stub, (void)caller_addr;
  return 0;
}

int sh_recorder_add_unhook(int error_number, uintptr_t stub, uintptr_t caller_addr) {
  (void)error_number, (void)stub, (void)caller_addr;
  return 0;
}

char *sh_recorder_get(uint32_t item_flags) {
  (void)item_flags;
  return NULL;
}

void sh_recorder_dump(int fd, uint32_t item_flags) {
  (void)fd, (void)item_flags;
}

#endif
