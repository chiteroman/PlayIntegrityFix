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

// Created by Yingmin Piao (piaoyingmin@bytedance.com) on 2022-09-26.

#include "bh_linker_mutex.h"

#include <pthread.h>

#if defined(__arm__) && __ANDROID_API__ < __ANDROID_API_L__

#include <dlfcn.h>
#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <unistd.h>

#include "bh_log.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-macros"
#define FIELD_MASK(shift, bits)           (((1 << (bits)) - 1) << (shift))
#define FIELD_TO_BITS(val, shift, bits)   (((val) & ((1 << (bits)) - 1)) << (shift))
#define FIELD_FROM_BITS(val, shift, bits) (((val) >> (shift)) & ((1 << (bits)) - 1))

#define MUTEX_SHARED_SHIFT 13
#define MUTEX_SHARED_MASK  FIELD_MASK(MUTEX_SHARED_SHIFT, 1)

#define MUTEX_TYPE_SHIFT          14
#define MUTEX_TYPE_LEN            2
#define MUTEX_TYPE_MASK           FIELD_MASK(MUTEX_TYPE_SHIFT, MUTEX_TYPE_LEN)
#define MUTEX_TYPE_RECURSIVE      1
#define MUTEX_TYPE_TO_BITS(t)     FIELD_TO_BITS(t, MUTEX_TYPE_SHIFT, MUTEX_TYPE_LEN)
#define MUTEX_TYPE_BITS_RECURSIVE MUTEX_TYPE_TO_BITS(MUTEX_TYPE_RECURSIVE)

#define MUTEX_OWNER_SHIFT 16
#define MUTEX_OWNER_LEN   16

#define MUTEX_OWNER_FROM_BITS(v) FIELD_FROM_BITS(v, MUTEX_OWNER_SHIFT, MUTEX_OWNER_LEN)
#pragma clang diagnostic pop

extern __attribute((weak)) unsigned long int getauxval(unsigned long int type);

static int get_dl_data_segment(size_t *begin, size_t *end) {
  if (getauxval == NULL) return -1;

  ElfW(Ehdr) *ehdr = (ElfW(Ehdr) *)(size_t)getauxval(AT_BASE);
  if (ehdr == NULL) return -1;

  if (0 != memcmp(ehdr->e_ident, ELFMAG, SELFMAG)) return -1;

  ElfW(Phdr) *phdr = (ElfW(Phdr) *)((size_t)ehdr + ehdr->e_phoff);

  for (int i = 0; i < ehdr->e_phnum; i++) {
    if (phdr->p_type == PT_LOAD && phdr->p_flags == (PF_R | PF_W)) {
      *begin = (size_t)ehdr + (size_t)phdr->p_vaddr;
      *end = *begin + (size_t)phdr->p_memsz;
      return 0;
    }
    phdr++;
  }

  return -1;
}

static int is_mutex_value(int *lock) {
  int value = *lock;
  if (value == MUTEX_TYPE_BITS_RECURSIVE) {
    return 1;
  }

  if ((value & MUTEX_TYPE_MASK) != MUTEX_TYPE_BITS_RECURSIVE || (value & MUTEX_SHARED_MASK)) {
    return 0;
  }

  if (gettid() == MUTEX_OWNER_FROM_BITS(value)) {
    return 1;
  }

  for (int i = 0; i < 1000; i++) {
    volatile int newval = *lock;
    if (newval == MUTEX_TYPE_BITS_RECURSIVE) {
      return 1;
    }
    usleep(1 * 1000);
  }
  return 0;
}

#define MIN_CODE_SIZE                128
#define MAX_COUNT_OF_STACK_CHECK     32
#define MAX_COUNT_IF_CALLEE_SAVE_REG 13  // r4..r15

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
static pthread_mutex_t *find_dl_lock_by_stack(void) {
  size_t begin = 0, end = 0;
  if (0 != get_dl_data_segment(&begin, &end) || begin >= end) {
    BH_LOG_ERROR("not found data segment!");
    return NULL;
  }

  void *somain = dlopen(NULL, RTLD_LOCAL);
  if (somain == NULL) {
    BH_LOG_ERROR("not found somain!");
    return NULL;
  }

  size_t stack[MAX_COUNT_OF_STACK_CHECK];
  size_t *cursp = NULL;

  /* don't modify this codes, even debug >>> */
  __asm__ volatile("mov %[_cur_sp], sp" : [_cur_sp] "=r"(cursp) : :);

  dlclose(somain);

  if (cursp == NULL) return NULL;

  cursp -= 1;

  for (size_t i = 0; i < MAX_COUNT_OF_STACK_CHECK; i++) {
    stack[i] = *(cursp - i);
  }
  /* <<< don't modify this codes, even debug */

#if 0
    BH_LOG_INFO("cursp=%p, dlclose=%p, begin=%p, end=%p",
            (void *)cursp, (void *)&dlclose, (void *)begin, (void *)end);

    for(size_t i = 0; i < MAX_COUNT_OF_STACK_CHECK; i++)
    {
        BH_LOG_INFO("[%p]=%p", (void *)(cursp - i), (void *)stack[i]);
    }
#endif

  for (size_t i = 0; i < MAX_COUNT_OF_STACK_CHECK; i++) {
    if (stack[i] > (size_t)&dlclose && stack[i] < (size_t)&dlclose + MIN_CODE_SIZE)  // found
    {
      size_t last = MAX_COUNT_OF_STACK_CHECK;

      if (i < MAX_COUNT_OF_STACK_CHECK - MAX_COUNT_IF_CALLEE_SAVE_REG) {
        last = i + MAX_COUNT_IF_CALLEE_SAVE_REG;
      }

      for (size_t j = 0; j < last; j++) {
        if ((stack[j] & 0x3) == 0 && stack[j] > begin && stack[j] < end) {
          if (is_mutex_value((int *)stack[j])) {
            return (pthread_mutex_t *)stack[j];
          }
        }
      }
      break;
    }
  }
  return NULL;
}
#pragma clang diagnostic pop

pthread_mutex_t *bh_linker_mutex_get_by_stack(void) {
  return find_dl_lock_by_stack();
}

#else

pthread_mutex_t *bh_linker_mutex_get_by_stack(void) {
  return NULL;
}

#endif
