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

#include "sh_task.h"

#include <inttypes.h>
#include <malloc.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "queue.h"
#include "sh_config.h"
#include "sh_errno.h"
#include "sh_linker.h"
#include "sh_log.h"
#include "sh_recorder.h"
#include "sh_sig.h"
#include "sh_switch.h"
#include "sh_util.h"
#include "shadowhook.h"
#include "xdl.h"

#define SH_TASK_THREAD_NAME "shadowhook-task"

// task
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
struct sh_task {
  char *lib_name;
  char *sym_name;
  uintptr_t target_addr;
  uintptr_t new_addr;
  uintptr_t *orig_addr;
  shadowhook_hooked_t hooked;
  void *hooked_arg;
  uintptr_t caller_addr;
  bool finished;
  bool error;
  bool ignore_symbol_check;
  TAILQ_ENTRY(sh_task, ) link;
};
#pragma clang diagnostic pop

// task queue
typedef TAILQ_HEAD(sh_task_queue, sh_task, ) sh_task_queue_t;

// task queue object
static sh_task_queue_t sh_tasks = TAILQ_HEAD_INITIALIZER(sh_tasks);
static pthread_rwlock_t sh_tasks_lock = PTHREAD_RWLOCK_INITIALIZER;
static int sh_tasks_unfinished_cnt = 0;

static int sh_task_eventfd;
static int thread_started_result = SHADOWHOOK_ERRNO_MONITOR_THREAD;

sh_task_t *sh_task_create_by_target_addr(uintptr_t target_addr, uintptr_t new_addr, uintptr_t *orig_addr,
                                         bool ignore_symbol_check, uintptr_t caller_addr) {
  sh_task_t *self = malloc(sizeof(sh_task_t));
  if (NULL == self) return NULL;
  self->lib_name = NULL;
  self->sym_name = NULL;
  self->target_addr = target_addr;
  self->new_addr = new_addr;
  self->orig_addr = orig_addr;
  self->hooked = NULL;
  self->hooked_arg = NULL;
  self->caller_addr = caller_addr;
  self->finished = false;
  self->error = false;
  self->ignore_symbol_check = ignore_symbol_check;

  return self;
}

sh_task_t *sh_task_create_by_sym_name(const char *lib_name, const char *sym_name, uintptr_t new_addr,
                                      uintptr_t *orig_addr, shadowhook_hooked_t hooked, void *hooked_arg,
                                      uintptr_t caller_addr) {
  sh_task_t *self = malloc(sizeof(sh_task_t));
  if (NULL == self) return NULL;

  if (NULL == (self->lib_name = strdup(lib_name))) goto err;
  if (NULL == (self->sym_name = strdup(sym_name))) goto err;
  self->target_addr = 0;
  self->new_addr = new_addr;
  self->orig_addr = orig_addr;
  self->hooked = hooked;
  self->hooked_arg = hooked_arg;
  self->caller_addr = caller_addr;
  self->finished = false;
  self->error = false;
  self->ignore_symbol_check = false;

  return self;

err:
  if (NULL != self->lib_name) free(self->lib_name);
  if (NULL != self->sym_name) free(self->sym_name);
  free(self);
  return NULL;
}

void sh_task_destroy(sh_task_t *self) {
  if (NULL != self->lib_name) free(self->lib_name);
  if (NULL != self->sym_name) free(self->sym_name);
  free(self);
}

static void sh_task_do_callback(sh_task_t *self, int error_number) {
  if (NULL != self->hooked)
    self->hooked(error_number, self->lib_name, self->sym_name, (void *)self->target_addr,
                 (void *)self->new_addr, self->orig_addr, self->hooked_arg);
}

static int sh_task_hook_pending(struct dl_phdr_info *info, size_t size, void *arg) {
  (void)size, (void)arg;

  pthread_rwlock_rdlock(&sh_tasks_lock);

  sh_task_t *task;
  TAILQ_FOREACH(task, &sh_tasks, link) {
    if (task->finished) continue;
    if ('/' == info->dlpi_name[0] && NULL == strstr(info->dlpi_name, task->lib_name)) continue;
    if ('/' != info->dlpi_name[0] && NULL == strstr(task->lib_name, info->dlpi_name)) continue;

    xdl_info_t dlinfo;
    char real_lib_name[512];
    int r = sh_linker_get_dlinfo_by_sym_name(task->lib_name, task->sym_name, &dlinfo, real_lib_name,
                                             sizeof(real_lib_name));
    task->target_addr = (uintptr_t)dlinfo.dli_saddr;
    if (SHADOWHOOK_ERRNO_PENDING != r) {
      size_t backup_len = 0;
      if (0 == r) {
        r = sh_switch_hook(task->target_addr, task->new_addr, task->orig_addr, &backup_len, &dlinfo);
        if (0 != r) task->error = true;
      } else {
        strlcpy(real_lib_name, task->lib_name, sizeof(real_lib_name));
        task->error = true;
      }
      sh_recorder_add_hook(r, false, task->target_addr, real_lib_name, task->sym_name, task->new_addr,
                           backup_len, (uintptr_t)task, task->caller_addr);
      task->finished = true;
      sh_task_do_callback(task, r);
      if (0 == __atomic_sub_fetch(&sh_tasks_unfinished_cnt, 1, __ATOMIC_SEQ_CST)) break;
    }
  }

  pthread_rwlock_unlock(&sh_tasks_lock);

  return __atomic_load_n(&sh_tasks_unfinished_cnt, __ATOMIC_SEQ_CST) > 0 ? 0 : 1;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

static void sh_task_post_dlopen_callback(void *arg) {
  (void)arg;

  if (0 == thread_started_result && __atomic_load_n(&sh_tasks_unfinished_cnt, __ATOMIC_SEQ_CST) > 0) {
    uint64_t ev_val = 1;
    SH_UTIL_TEMP_FAILURE_RETRY(write(sh_task_eventfd, &ev_val, sizeof(ev_val)));
  }
}

__noreturn static void *sh_task_thread_func(void *arg) {
  (void)arg;
  pthread_t thread = pthread_self();
  pthread_setname_np(thread, SH_TASK_THREAD_NAME);
  pthread_detach(thread);

  struct pollfd ev = {.fd = sh_task_eventfd, .events = POLLIN, .revents = 0};
  while (1) {
    int n = SH_UTIL_TEMP_FAILURE_RETRY(poll(&ev, 1, -1));
    if (n < 0) {
      sleep(1);
      continue;
    } else if (n > 0) {
      uint64_t ev_val;
      SH_UTIL_TEMP_FAILURE_RETRY(read(sh_task_eventfd, &ev_val, sizeof(ev_val)));

      if (sh_util_get_api_level() >= __ANDROID_API_L__) {
        xdl_iterate_phdr(sh_task_hook_pending, NULL, XDL_DEFAULT);
      } else {
        SH_SIG_TRY(SIGSEGV, SIGBUS) {
          xdl_iterate_phdr(sh_task_hook_pending, NULL, XDL_DEFAULT);
        }
        SH_SIG_CATCH() {
          SH_LOG_WARN("task: dliterate crashed");
        }
        SH_SIG_EXIT
      }
    }
  }
}

#pragma clang diagnostic pop

static int sh_task_start_monitor(bool start_thread) {
  static bool thread_started = false;
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
  pthread_t thread;
  int r;

  // hook linker dlopen()
  if (0 != (r = sh_linker_hook_dlopen(sh_task_post_dlopen_callback, NULL))) return r;

  if (!start_thread) return 0;

  // start thread
  if (thread_started) return thread_started_result;
  pthread_mutex_lock(&lock);
  if (thread_started) goto end;

  if (0 > (sh_task_eventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC))) goto end;
  if (0 != pthread_create(&thread, NULL, &sh_task_thread_func, NULL)) goto end;

  // OK
  thread_started_result = 0;

end:
  thread_started = true;
  pthread_mutex_unlock(&lock);
  SH_LOG_INFO("task: start monitor %s, return: %d", 0 == thread_started_result ? "OK" : "FAILED",
              thread_started_result);
  return thread_started_result;
}

int sh_task_hook(sh_task_t *self) {
  int r;
  bool is_hook_sym_addr = true;
  char real_lib_name[512] = "unknown";
  char real_sym_name[1024] = "unknown";
  size_t backup_len = 0;

  // find target-address by library-name and symbol-name
  xdl_info_t dlinfo;
  memset(&dlinfo, 0, sizeof(xdl_info_t));
  if (0 == self->target_addr) {
    is_hook_sym_addr = false;
    strlcpy(real_lib_name, self->lib_name, sizeof(real_lib_name));
    strlcpy(real_sym_name, self->sym_name, sizeof(real_sym_name));
    r = sh_linker_get_dlinfo_by_sym_name(self->lib_name, self->sym_name, &dlinfo, real_lib_name,
                                         sizeof(real_lib_name));
    if (SHADOWHOOK_ERRNO_PENDING == r) {
      // we need to start monitor linker dlopen for handle the pending task
      if (0 != (r = sh_task_start_monitor(true))) goto end;
      r = SHADOWHOOK_ERRNO_PENDING;
      goto end;
    }
    if (0 != r) goto end;                             // error
    self->target_addr = (uintptr_t)dlinfo.dli_saddr;  // OK
  } else {
    r = sh_linker_get_dlinfo_by_addr((void *)self->target_addr, &dlinfo, real_lib_name, sizeof(real_lib_name),
                                     real_sym_name, sizeof(real_sym_name), self->ignore_symbol_check);
    if (0 != r) goto end;  // error
  }

  // In UNIQUE mode, if external users are hooking the linker dlopen() or do_dlopen(),
  // we MUST hook this method with invisible for ourself first.
  if (sh_linker_need_to_hook_dlopen(self->target_addr)) {
    SH_LOG_INFO("task: hook dlopen/do_dlopen internal. target-address %" PRIxPTR, self->target_addr);
    if (0 != (r = sh_task_start_monitor(false))) goto end;
  }

  // hook by target-address
  r = sh_switch_hook(self->target_addr, self->new_addr, self->orig_addr, &backup_len, &dlinfo);
  self->finished = true;

end:
  if (0 == r || SHADOWHOOK_ERRNO_PENDING == r)  // "PENDING" is NOT an error
  {
    pthread_rwlock_wrlock(&sh_tasks_lock);
    TAILQ_INSERT_TAIL(&sh_tasks, self, link);
    if (!self->finished) __atomic_add_fetch(&sh_tasks_unfinished_cnt, 1, __ATOMIC_SEQ_CST);
    pthread_rwlock_unlock(&sh_tasks_lock);
  }

  // record
  sh_recorder_add_hook(r, is_hook_sym_addr, self->target_addr, real_lib_name, real_sym_name, self->new_addr,
                       backup_len, (uintptr_t)self, self->caller_addr);

  return r;
}

int sh_task_unhook(sh_task_t *self, uintptr_t caller_addr) {
  pthread_rwlock_wrlock(&sh_tasks_lock);
  TAILQ_REMOVE(&sh_tasks, self, link);
  if (!self->finished) __atomic_sub_fetch(&sh_tasks_unfinished_cnt, 1, __ATOMIC_SEQ_CST);
  pthread_rwlock_unlock(&sh_tasks_lock);

  // check task status
  int r;
  if (self->error) {
    r = SHADOWHOOK_ERRNO_UNHOOK_ON_ERROR;
    goto end;
  }
  if (!self->finished) {
    r = SHADOWHOOK_ERRNO_UNHOOK_ON_UNFINISHED;
    goto end;
  }

  // do unhook
  r = sh_switch_unhook(self->target_addr, self->new_addr);

end:
  // record
  sh_recorder_add_unhook(r, (uintptr_t)self, caller_addr);
  return r;
}
