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

// Created by Li Zhang (zhangli.foxleezh@bytedance.com) on 2020-06-21.

#include "bh_trampo.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "bh_hook.h"
#include "bh_log.h"
#include "bh_util.h"
#include "bytehook.h"
#include "bytesig.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreserved-id-macro"
#pragma clang diagnostic ignored "-Wvariadic-macros"
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wpacked"
#pragma clang diagnostic ignored "-Wshorten-64-to-32"
#include "linux_syscall_support.h"
#pragma clang diagnostic pop

#define BH_TRAMPO_BLOCK_NAME      "bytehook-plt-trampolines"
#define BH_TRAMPO_BLOCK_SIZE      4096
#define BH_TRAMPO_ALIGN           4
#define BH_TRAMPO_STACK_NAME      "bytehook-stack"
#define BH_TRAMPO_STACK_SIZE      4096
#define BH_TRAMPO_STACK_FRAME_MAX 16
#define BH_TRAMPO_THREAD_MAX      1024

typedef struct {
  bh_hook_call_list_t proxies;
  void *orig_func;
  void *return_address;
} bh_trampo_frame_t;

typedef struct {
  size_t frames_cnt;
  bh_trampo_frame_t frames[BH_TRAMPO_STACK_FRAME_MAX];
} bh_trampo_stack_t;

static pthread_key_t bh_trampo_tls_key;
static bh_trampo_stack_t bh_hub_stack_cache[BH_TRAMPO_THREAD_MAX];
static uint8_t bh_hub_stack_cache_used[BH_TRAMPO_THREAD_MAX];

static bh_trampo_stack_t *bh_trampo_stack_create(void) {
  // get stack from global cache
  for (size_t i = 0; i < BH_TRAMPO_THREAD_MAX; i++) {
    uint8_t *used = &(bh_hub_stack_cache_used[i]);
    if (0 == *used) {
      uint8_t expected = 0;
      if (__atomic_compare_exchange_n(used, &expected, 1, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        bh_trampo_stack_t *stack = &(bh_hub_stack_cache[i]);
        stack->frames_cnt = 0;
        return stack;  // OK
      }
    }
  }

  // create new stack by mmap
  void *buf =
      sys_mmap(NULL, BH_TRAMPO_STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (MAP_FAILED == buf) return NULL;  // failed
  sys_prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, (unsigned long)buf, BH_TRAMPO_STACK_SIZE,
            (unsigned long)BH_TRAMPO_STACK_NAME);
  bh_trampo_stack_t *stack = (bh_trampo_stack_t *)buf;
  stack->frames_cnt = 0;
  return stack;  // OK
}

static void bh_trampo_stack_destroy(void *buf) {
  if (NULL == buf) return;

  if ((uintptr_t)bh_hub_stack_cache <= (uintptr_t)buf &&
      (uintptr_t)buf < ((uintptr_t)bh_hub_stack_cache + sizeof(bh_hub_stack_cache))) {
    // return stack to global cache
    size_t i = ((uintptr_t)buf - (uintptr_t)bh_hub_stack_cache) / sizeof(bh_trampo_stack_t);
    uint8_t *used = &(bh_hub_stack_cache_used[i]);
    if (1 != *used) abort();
    __atomic_store_n(used, 0, __ATOMIC_RELEASE);
  } else {
    // munmap stack
    munmap(buf, BH_TRAMPO_STACK_SIZE);
  }
}

int bh_trampo_init(void) {
  if (0 != pthread_key_create(&bh_trampo_tls_key, bh_trampo_stack_destroy)) return -1;
  memset(&bh_hub_stack_cache, 0, sizeof(bh_hub_stack_cache));
  memset(&bh_hub_stack_cache_used, 0, sizeof(bh_hub_stack_cache_used));
  return 0;
}

static void *bh_trampo_push_stack(bh_hook_t *hook, void *return_address) {
  bh_trampo_stack_t *stack = (bh_trampo_stack_t *)pthread_getspecific(bh_trampo_tls_key);

  // create TLS data, only once
  if (__predict_false(NULL == stack)) {
    if (__predict_false(NULL == (stack = bh_trampo_stack_create()))) goto end;
    pthread_setspecific(bh_trampo_tls_key, (void *)stack);
  }

  // check whether a recursive call occurred
  bool recursive = false;
  for (size_t i = stack->frames_cnt; i > 0; i--) {
    bh_trampo_frame_t *frame = &stack->frames[i - 1];

    if (frame->orig_func == hook->orig_func) {
      // recursive call found
      recursive = true;
      break;
    }
  }

  // find and return the first enabled hook-function in the hook-chain
  // (does not include the original function)
  if (!recursive) {
    bh_hook_call_t *running;
    SLIST_FOREACH(running, &hook->running_list, link) {
      if (running->enabled) {
        // push a new frame for the current proxy
        if (stack->frames_cnt >= BH_TRAMPO_STACK_FRAME_MAX) goto end;
        stack->frames_cnt++;
        bh_trampo_frame_t *frame = &stack->frames[stack->frames_cnt - 1];
        frame->proxies = hook->running_list;
        frame->orig_func = hook->orig_func;
        frame->return_address = return_address;

        return running->func;
      }
    }
  }

  // if not found enabled hook-function in the hook-chain, or recursive call found,
  // just return the original-function
end:
  return hook->orig_func;
}

static void *bh_trampo_allocate(size_t sz) {
  // current trampo block and info
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
  static void *block = NULL;
  static size_t remaining = 0;

  // return value
  void *ret;

  // round sz up to nearest 4-bytes boundary
  sz = (sz + ((size_t)BH_TRAMPO_ALIGN - 1)) & ~((size_t)BH_TRAMPO_ALIGN - 1);

  pthread_mutex_lock(&lock);

  // get/create an usable block
  if (remaining < sz) {
    // create new memory map
    block = sys_mmap(NULL, BH_TRAMPO_BLOCK_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (MAP_FAILED == block) {
      ret = NULL;
      goto end;
    }
    prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, block, BH_TRAMPO_BLOCK_SIZE, BH_TRAMPO_BLOCK_NAME);

    // update the remaining size info
    remaining = BH_TRAMPO_BLOCK_SIZE;

    BH_LOG_INFO("trampo block: created at %" PRIxPTR ", size %d", (uintptr_t)block, BH_TRAMPO_BLOCK_SIZE);
  }

  // got it
  ret = (void *)((size_t)block + BH_TRAMPO_BLOCK_SIZE - remaining);
  remaining -= sz;

end:
  pthread_mutex_unlock(&lock);
  return ret;
}

static void *bh_trampo_template_pointer(void) {
#if defined(__arm__) && defined(__thumb__)
  return (void *)((uintptr_t)&bh_trampo_template - 1);
#else
  return (void *)&bh_trampo_template;
#endif
}

void *bh_trampo_create(bh_hook_t *hook) {
  size_t code_size = (uintptr_t)(&bh_trampo_data) - (uintptr_t)(bh_trampo_template_pointer());
  size_t data_size = sizeof(void *) + sizeof(void *);

  // create trampoline
  void *trampo = bh_trampo_allocate(code_size + data_size);
  if (NULL == trampo) return NULL;

  // fill in code
  BYTESIG_TRY(SIGSEGV, SIGBUS) {
    memcpy(trampo, bh_trampo_template_pointer(), code_size);
  }
  BYTESIG_CATCH() {
    return NULL;
  }
  BYTESIG_EXIT

  // file in data
  void **data = (void **)((uintptr_t)trampo + code_size);
  *data++ = (void *)bh_trampo_push_stack;
  *data = (void *)hook;

  // clear CPU cache
  __builtin___clear_cache((char *)trampo, (char *)trampo + code_size + data_size);

  BH_LOG_INFO("trampo: created for GOT %" PRIxPTR " at %" PRIxPTR ", size %zu + %zu = %zu",
              (uintptr_t)hook->got_addr, (uintptr_t)trampo, code_size, data_size, code_size + data_size);

#if defined(__arm__) && defined(__thumb__)
  trampo = (void *)((uintptr_t)trampo + 1);
#endif
  return trampo;
}

void *bh_trampo_get_prev_func(void *func) {
  bh_trampo_stack_t *stack = (bh_trampo_stack_t *)pthread_getspecific(bh_trampo_tls_key);
  if (0 == stack->frames_cnt) abort();  // called in a non-hook status?
  bh_trampo_frame_t *frame = &stack->frames[stack->frames_cnt - 1];

  // find and return the next enabled hook-function in the hook-chain
  bool found = false;
  bh_hook_call_t *running;
  SLIST_FOREACH(running, &(frame->proxies), link) {
    if (!found) {
      if (running->func == func) found = true;
    } else {
      if (running->enabled) break;
    }
  }
  if (NULL != running) return running->func;

  // did not find, return the original-function
  return frame->orig_func;
}

void bh_trampo_pop_stack(void *return_address) {
  bh_trampo_stack_t *stack = (bh_trampo_stack_t *)pthread_getspecific(bh_trampo_tls_key);
  if (0 == stack->frames_cnt) return;
  bh_trampo_frame_t *frame = &stack->frames[stack->frames_cnt - 1];

  if (return_address == frame->return_address) stack->frames_cnt--;
}

void *bh_trampo_get_return_address(void) {
  bh_trampo_stack_t *stack = (bh_trampo_stack_t *)pthread_getspecific(bh_trampo_tls_key);
  if (0 == stack->frames_cnt) abort();  // called in a non-hook status?
  bh_trampo_frame_t *frame = &stack->frames[stack->frames_cnt - 1];

  return frame->return_address;
}
