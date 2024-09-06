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

#include "bh_task_manager.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "bh_core.h"
#include "bh_dl_monitor.h"
#include "bh_elf_manager.h"
#include "bh_log.h"
#include "bh_task.h"
#include "queue.h"

typedef TAILQ_HEAD(bh_task_queue, bh_task, ) bh_task_queue_t;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
struct bh_task_manager {
  bh_task_queue_t tasks;
  pthread_rwlock_t lock;
};
#pragma clang diagnostic pop

bh_task_manager_t *bh_task_manager_create(void) {
  bh_task_manager_t *self = malloc(sizeof(bh_task_manager_t));
  if (NULL == self) return NULL;
  TAILQ_INIT(&self->tasks);
  pthread_rwlock_init(&self->lock, NULL);
  return self;
}

void bh_task_manager_add(bh_task_manager_t *self, bh_task_t *task) {
  pthread_rwlock_wrlock(&self->lock);
  TAILQ_INSERT_TAIL(&self->tasks, task, link);
  pthread_rwlock_unlock(&self->lock);
}

void bh_task_manager_del(bh_task_manager_t *self, bh_task_t *task) {
  pthread_rwlock_wrlock(&self->lock);
  TAILQ_REMOVE(&self->tasks, task, link);
  pthread_rwlock_unlock(&self->lock);
}

static void bh_task_manager_post_new_elf(bh_elf_t *elf, void *arg) {
  BH_LOG_INFO("task manager: try hook in new ELF: %s", elf->pathname);
  bh_task_manager_t *self = (bh_task_manager_t *)arg;

  pthread_rwlock_rdlock(&self->lock);
  bh_task_t *task;
  TAILQ_FOREACH(task, &self->tasks, link) {
    bh_task_hook_elf(task, elf);
  }
  pthread_rwlock_unlock(&self->lock);
}

static void bh_task_manager_post_dlopen(void *arg) {
  BH_LOG_INFO("task manager: post dlopen() OK");

  bh_dl_monitor_dlclose_rdlock();
  bh_elf_manager_refresh(bh_core_global()->elf_mgr, false, bh_task_manager_post_new_elf, arg);
  bh_dl_monitor_dlclose_unlock();
}

static void bh_task_manager_post_dlclose(bool sync_refresh, void *arg) {
  (void)arg;
  BH_LOG_INFO("task manager: post dlclose() OK, sync_refresh: %d", sync_refresh);

  if (sync_refresh) {
    // in the range of dl_monitor's write-lock
    bh_elf_manager_refresh(bh_core_global()->elf_mgr, true, NULL, NULL);
  } else {
    bh_dl_monitor_dlclose_rdlock();
    bh_elf_manager_refresh(bh_core_global()->elf_mgr, false, NULL, NULL);
    bh_dl_monitor_dlclose_unlock();
  }
}

static int bh_task_manager_init_dl_monitor(bh_task_manager_t *self) {
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
  static bool inited = false;
  static bool inited_ok = false;

  if (inited) return inited_ok ? 0 : -1;  // Do not repeat the initialization.

  int r;
  pthread_mutex_lock(&lock);
  if (!inited) {
    bh_dl_monitor_set_post_dlopen(bh_task_manager_post_dlopen, self);
    bh_dl_monitor_set_post_dlclose(bh_task_manager_post_dlclose, NULL);
    if (0 == (r = bh_dl_monitor_init())) inited_ok = true;
    inited = true;
  } else {
    r = inited_ok ? 0 : -1;
  }
  pthread_mutex_unlock(&lock);
  return r;
}

void bh_task_manager_hook(bh_task_manager_t *self, bh_task_t *task) {
  if (bh_dl_monitor_is_initing()) {
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    static bool oneshot_refreshed = false;
    if (!oneshot_refreshed) {
      bool hooked = false;
      pthread_mutex_lock(&lock);
      if (!oneshot_refreshed) {
        bh_dl_monitor_dlclose_rdlock();
        bh_elf_manager_refresh(bh_core_global()->elf_mgr, false, NULL, NULL);
        bh_task_hook(task);
        bh_dl_monitor_dlclose_unlock();
        oneshot_refreshed = true;
        hooked = true;
      }
      pthread_mutex_unlock(&lock);
      if (hooked) return;
    }
  } else {
    // start & check dl-monitor
    if (0 != bh_task_manager_init_dl_monitor(self)) {
      // For internal tasks in the DL monitor, this is not an error.
      // But these internal tasks do not set callbacks, so there will be no side effects.
      bh_task_hooked(task, BYTEHOOK_STATUS_CODE_INITERR_DLMTR, NULL, NULL);
      return;
    }
  }

  bh_dl_monitor_dlclose_rdlock();
  bh_task_hook(task);
  bh_dl_monitor_dlclose_unlock();
}

int bh_task_manager_unhook(bh_task_manager_t *self, bh_task_t *task) {
  (void)self;

  bh_dl_monitor_dlclose_rdlock();
  int r = bh_task_unhook(task);
  bh_dl_monitor_dlclose_unlock();
  return r;
}
