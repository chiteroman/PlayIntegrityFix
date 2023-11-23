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

#include "sh_switch.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sh_config.h"
#include "sh_errno.h"
#include "sh_hub.h"
#include "sh_inst.h"
#include "sh_linker.h"
#include "sh_log.h"
#include "sh_safe.h"
#include "sh_sig.h"
#include "sh_util.h"
#include "shadowhook.h"
#include "tree.h"
#include "xdl.h"

// switch
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct sh_switch {
  sh_inst_t inst;  // align 16
  uintptr_t target_addr;
  sh_hub_t *hub;
  RB_ENTRY(sh_switch) link;
} sh_switch_t;
#pragma clang diagnostic pop

// switch tree
static __inline__ int sh_switch_cmp(sh_switch_t *a, sh_switch_t *b) {
  if (a->target_addr == b->target_addr)
    return 0;
  else
    return a->target_addr > b->target_addr ? 1 : -1;
}
typedef RB_HEAD(sh_switch_tree, sh_switch) sh_switch_tree_t;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
RB_GENERATE_STATIC(sh_switch_tree, sh_switch, link, sh_switch_cmp)
#pragma clang diagnostic pop

// switch tree object
static sh_switch_tree_t sh_switches = RB_INITIALIZER(&sh_switches);
static pthread_rwlock_t sh_switches_lock = PTHREAD_RWLOCK_INITIALIZER;

static sh_switch_t *sh_switch_find(uintptr_t target_addr) {
  sh_switch_t key = {.target_addr = target_addr};

  pthread_rwlock_rdlock(&sh_switches_lock);
  sh_switch_t *self = RB_FIND(sh_switch_tree, &sh_switches, &key);
  pthread_rwlock_unlock(&sh_switches_lock);

  return self;
}

static int sh_switch_create(sh_switch_t **self, uintptr_t target_addr, uintptr_t *hub_trampo) {
  *self = memalign(16, sizeof(sh_switch_t));
  if (NULL == *self) return SHADOWHOOK_ERRNO_OOM;

  memset(&(*self)->inst, 0, sizeof((*self)->inst));
  (*self)->target_addr = target_addr;
  (*self)->hub = NULL;

  if (NULL != hub_trampo) {
    if (NULL == ((*self)->hub = sh_hub_create(target_addr, hub_trampo))) {
      free(self);
      return SHADOWHOOK_ERRNO_HUB_CREAT;
    }
  }

  return 0;
}

static void sh_switch_destroy(sh_switch_t *self, bool hub_with_delay) {
  if (NULL != self->hub) sh_hub_destroy(self->hub, hub_with_delay);
  free(self);
}

static void sh_switch_dump_enter(sh_switch_t *self) {
#ifdef SH_CONFIG_DEBUG
  char buf[1024];
  size_t len = 0;
  size_t zero = 0;
  for (size_t i = 0; i < 128; i++) {
    uint16_t inst = ((uint16_t *)(self->inst.enter_addr))[i];
    zero = (0 == inst ? zero + 1 : 0);
    if (zero > 4) break;
    len += (size_t)snprintf(buf + len, sizeof(buf) - len, "%04" PRIx16 " ", inst);
  }
  SH_LOG_DEBUG("switch: enter < %s>", buf);
#else
  (void)self;
#endif
}

static int sh_switch_hook_unique(uintptr_t target_addr, uintptr_t new_addr, uintptr_t *orig_addr,
                                 size_t *backup_len, xdl_info_t *dlinfo) {
  sh_switch_t *self = sh_switch_find(target_addr);
  if (NULL != self) return SHADOWHOOK_ERRNO_HOOK_DUP;

  // alloc new switch
  int r;
  if (0 != (r = sh_switch_create(&self, target_addr, NULL))) return r;

  sh_switch_t *useless = NULL;
  pthread_rwlock_wrlock(&sh_switches_lock);  // SYNC - start

  // insert new switch to switch-tree
  if (NULL != RB_INSERT(sh_switch_tree, &sh_switches, self)) {
    useless = self;
    r = SHADOWHOOK_ERRNO_HOOK_DUP;
    goto end;
  }

  // do hook
  if (0 != (r = sh_inst_hook(&self->inst, target_addr, dlinfo, new_addr, orig_addr, NULL))) {
    RB_REMOVE(sh_switch_tree, &sh_switches, self);
    useless = self;
    goto end;
  }
  *backup_len = self->inst.backup_len;
  sh_switch_dump_enter(self);

end:
  pthread_rwlock_unlock(&sh_switches_lock);  // SYNC - end
  if (NULL != useless) sh_switch_destroy(useless, false);
  return r;
}

static int sh_switch_hook_shared(uintptr_t target_addr, uintptr_t new_addr, uintptr_t *orig_addr,
                                 size_t *backup_len, xdl_info_t *dlinfo) {
  int r;

  pthread_rwlock_rdlock(&sh_switches_lock);  // SYNC(read) - start
  sh_switch_t key = {.target_addr = target_addr};
  sh_switch_t *self = RB_FIND(sh_switch_tree, &sh_switches, &key);
  if (NULL != self)  // already exists
  {
    // add an new proxy to hub
    if (NULL != orig_addr) *orig_addr = sh_hub_get_orig_addr(self->hub);
    r = sh_hub_add_proxy(self->hub, new_addr);
    pthread_rwlock_unlock(&sh_switches_lock);  // SYNC(read) - end

    *backup_len = self->inst.backup_len;
    return r;
  }
  pthread_rwlock_unlock(&sh_switches_lock);  // SYNC(read) - end

  // first hook for this target_addr

  // alloc new switch
  uintptr_t hub_trampo;
  if (0 != (r = sh_switch_create(&self, target_addr, &hub_trampo))) return r;

  sh_switch_t *useless = NULL;
  pthread_rwlock_wrlock(&sh_switches_lock);  // SYNC - start

  // insert new switch to switch-tree
  sh_switch_t *exists;
  if (NULL != (exists = RB_INSERT(sh_switch_tree, &sh_switches, self))) {
    // already exists
    useless = self;
    if (NULL != orig_addr) *orig_addr = sh_hub_get_orig_addr(exists->hub);
    r = sh_hub_add_proxy(exists->hub, new_addr);
    *backup_len = exists->inst.backup_len;
  } else {
    // do hook
    uintptr_t *safe_orig_addr_addr = sh_safe_get_orig_addr_addr(target_addr);
    if (0 != (r = sh_inst_hook(&self->inst, target_addr, dlinfo, hub_trampo,
                               sh_hub_get_orig_addr_addr(self->hub), safe_orig_addr_addr))) {
      RB_REMOVE(sh_switch_tree, &sh_switches, self);
      useless = self;
      goto end;
    }
    *backup_len = self->inst.backup_len;
    sh_switch_dump_enter(self);

    // return original-address
    if (NULL != orig_addr) *orig_addr = sh_hub_get_orig_addr(self->hub);

    // add proxy to hub
    if (0 != (r = sh_hub_add_proxy(self->hub, new_addr))) {
      sh_inst_unhook(&self->inst, target_addr);
      *backup_len = 0;
      RB_REMOVE(sh_switch_tree, &sh_switches, self);
      useless = self;
      goto end;
    }
  }

end:
  pthread_rwlock_unlock(&sh_switches_lock);  // SYNC - end
  if (NULL != useless) sh_switch_destroy(useless, false);

  return r;
}

int sh_switch_hook(uintptr_t target_addr, uintptr_t new_addr, uintptr_t *orig_addr, size_t *backup_len,
                   xdl_info_t *dlinfo) {
  int r;
  if (SHADOWHOOK_IS_UNIQUE_MODE)
    r = sh_switch_hook_unique(target_addr, new_addr, orig_addr, backup_len, dlinfo);
  else
    r = sh_switch_hook_shared(target_addr, new_addr, orig_addr, backup_len, dlinfo);

  if (0 == r)
    SH_LOG_INFO("switch: hook in %s mode OK: target_addr %" PRIxPTR ", new_addr %" PRIxPTR,
                SHADOWHOOK_IS_UNIQUE_MODE ? "UNIQUE" : "SHARED", target_addr, new_addr);

  return r;
}

static int sh_switch_hook_unique_invisible(uintptr_t target_addr, uintptr_t new_addr, uintptr_t *orig_addr,
                                           size_t *backup_len, xdl_info_t *dlinfo) {
  pthread_rwlock_wrlock(&sh_switches_lock);  // SYNC - start

  // do hook
  sh_inst_t inst;
  int r = sh_inst_hook(&inst, target_addr, dlinfo, new_addr, orig_addr, NULL);

  pthread_rwlock_unlock(&sh_switches_lock);  // SYNC - end

  *backup_len = inst.backup_len;
  return r;
}

int sh_switch_hook_invisible(uintptr_t target_addr, uintptr_t new_addr, uintptr_t *orig_addr,
                             size_t *backup_len, xdl_info_t *dlinfo) {
  int r;
  if (SHADOWHOOK_IS_UNIQUE_MODE)
    r = sh_switch_hook_unique_invisible(target_addr, new_addr, orig_addr, backup_len, dlinfo);
  else
    r = sh_switch_hook_shared(target_addr, new_addr, orig_addr, backup_len, dlinfo);

  if (0 == r)
    SH_LOG_INFO("switch: hook(invisible) in %s mode OK: target_addr %" PRIxPTR ", new_addr %" PRIxPTR,
                SHADOWHOOK_IS_UNIQUE_MODE ? "UNIQUE" : "SHARED", target_addr, new_addr);
  return r;
}

static int sh_switch_unhook_unique(uintptr_t target_addr) {
  int r;
  sh_switch_t *useless = NULL;

  pthread_rwlock_wrlock(&sh_switches_lock);  // SYNC - start

  sh_switch_t key = {.target_addr = target_addr};
  sh_switch_t *self = RB_FIND(sh_switch_tree, &sh_switches, &key);
  if (NULL == self) {
    r = SHADOWHOOK_ERRNO_UNHOOK_NOTFOUND;
    goto end;
  }
  r = sh_inst_unhook(&self->inst, target_addr);
  RB_REMOVE(sh_switch_tree, &sh_switches, self);
  useless = self;

end:
  pthread_rwlock_unlock(&sh_switches_lock);  // SYNC - end
  if (NULL != useless) sh_switch_destroy(useless, false);
  return r;
}

static int sh_switch_unhook_shared(uintptr_t target_addr, uintptr_t new_addr) {
  int r;
  sh_switch_t *useless = NULL;

  pthread_rwlock_wrlock(&sh_switches_lock);  // SYNC - start

  sh_switch_t key = {.target_addr = target_addr};
  sh_switch_t *self = RB_FIND(sh_switch_tree, &sh_switches, &key);
  if (NULL == self) {
    r = SHADOWHOOK_ERRNO_UNHOOK_NOTFOUND;
    goto end;
  }

  // delete proxy in hub
  bool have_enabled_proxy;
  if (0 != sh_hub_del_proxy(self->hub, new_addr, &have_enabled_proxy)) {
    r = SHADOWHOOK_ERRNO_UNHOOK_NOTFOUND;
    goto end;
  }
  r = 0;

  // unhook inst, remove current switch from switch-tree
  if (!have_enabled_proxy) {
    r = sh_inst_unhook(&self->inst, target_addr);

    uintptr_t *safe_orig_addr_addr = sh_safe_get_orig_addr_addr(target_addr);
    if (NULL != safe_orig_addr_addr) __atomic_store_n(safe_orig_addr_addr, 0, __ATOMIC_SEQ_CST);

    RB_REMOVE(sh_switch_tree, &sh_switches, self);
    useless = self;
  }

end:
  pthread_rwlock_unlock(&sh_switches_lock);  // SYNC - end
  if (NULL != useless) sh_switch_destroy(useless, true);
  return r;
}

int sh_switch_unhook(uintptr_t target_addr, uintptr_t new_addr) {
  int r;
  if (SHADOWHOOK_IS_UNIQUE_MODE) {
    r = sh_switch_unhook_unique(target_addr);
    if (0 == r) SH_LOG_INFO("switch: unhook in UNIQUE mode OK: target_addr %" PRIxPTR, target_addr);
  } else {
    r = sh_switch_unhook_shared(target_addr, new_addr);
    if (0 == r)
      SH_LOG_INFO("switch: unhook in SHARED mode OK: target_addr %" PRIxPTR ", new_addr %" PRIxPTR,
                  target_addr, new_addr);
  }

  return r;
}
