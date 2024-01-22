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

#include "sh_exit.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "sh_config.h"
#include "sh_log.h"
#include "sh_sig.h"
#include "sh_trampo.h"
#include "sh_util.h"
#include "shadowhook.h"
#include "xdl.h"

#define SH_EXIT_PAGE_NAME "shadowhook-exit"
#if defined(__arm__)
#define SH_EXIT_SZ 8
#elif defined(__aarch64__)
#define SH_EXIT_SZ 16
#endif
#define SH_EXIT_DELAY_SEC 2
#define SH_EXIT_GAPS_CAP  16

// Used to identify whether the ELF gap has been zero-filled,
// and also serves as a guard instruction.
#if defined(__arm__)
#define SH_EXIT_CLEAR_FLAG 0xE1200070BE00BE00  // BKPT #0(thumb) + BKPT #0(thumb) + BKPT #0(arm)
#elif defined(__aarch64__)
#define SH_EXIT_CLEAR_FLAG 0xD4200000D4200000  // BRK #0 + BRK #0
#endif

#define SH_EXIT_TYPE_OUT_LIBRARY 0
#define SH_EXIT_TYPE_IN_LIBRARY  1

extern __attribute((weak)) unsigned long int getauxval(unsigned long int);

static pthread_mutex_t sh_exit_lock = PTHREAD_MUTEX_INITIALIZER;
static sh_trampo_mgr_t sh_exit_trampo_mgr;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct {
  uintptr_t load_bias;
  const ElfW(Phdr) *dlpi_phdr;
  ElfW(Half) dlpi_phnum;
} sh_exit_elfinfo_t;
#pragma clang diagnostic pop

static sh_exit_elfinfo_t sh_exit_app_process_info;
static sh_exit_elfinfo_t sh_exit_linker_info;
static sh_exit_elfinfo_t sh_exit_vdso_info;  // vdso may not exist

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"

static void sh_exit_init_elfinfo(unsigned long type, sh_exit_elfinfo_t *info) {
  if (__predict_false(NULL == getauxval)) goto err;

  uintptr_t val = (uintptr_t)getauxval(type);
  if (__predict_false(0 == val)) goto err;

  // get base
  uintptr_t base = (AT_PHDR == type ? (val & (~0xffful)) : val);
  if (__predict_false(0 != memcmp((void *)base, ELFMAG, SELFMAG))) goto err;

  // ELF info
  ElfW(Ehdr) *ehdr = (ElfW(Ehdr) *)base;
  const ElfW(Phdr) *dlpi_phdr = (const ElfW(Phdr) *)(base + ehdr->e_phoff);
  ElfW(Half) dlpi_phnum = ehdr->e_phnum;

  // get load_bias
  uintptr_t min_vaddr = UINTPTR_MAX;
  for (size_t i = 0; i < dlpi_phnum; i++) {
    const ElfW(Phdr) *phdr = &(dlpi_phdr[i]);
    if (PT_LOAD == phdr->p_type) {
      if (min_vaddr > phdr->p_vaddr) min_vaddr = phdr->p_vaddr;
    }
  }
  if (__predict_false(UINTPTR_MAX == min_vaddr || base < min_vaddr)) goto err;
  uintptr_t load_bias = base - min_vaddr;

  info->load_bias = load_bias;
  info->dlpi_phdr = dlpi_phdr;
  info->dlpi_phnum = dlpi_phnum;
  return;

err:
  info->load_bias = 0;
  info->dlpi_phdr = NULL;
  info->dlpi_phnum = 0;
}

void sh_exit_init(void) {
  // init for out-library mode
  sh_trampo_init_mgr(&sh_exit_trampo_mgr, SH_EXIT_PAGE_NAME, SH_EXIT_SZ, SH_EXIT_DELAY_SEC);

  // init for in-library mode
  sh_exit_init_elfinfo(AT_PHDR, &sh_exit_app_process_info);
  sh_exit_init_elfinfo(AT_BASE, &sh_exit_linker_info);
  sh_exit_init_elfinfo(AT_SYSINFO_EHDR, &sh_exit_vdso_info);
}

// out-library mode:
//
// We store the shellcode for exit in mmaped memory near the PC.
//

static int sh_exit_alloc_out_library(uintptr_t *exit_addr, uintptr_t pc, xdl_info_t *dlinfo, uint8_t *exit,
                                     size_t exit_len, size_t range_low, size_t range_high) {
  (void)dlinfo;

  uintptr_t addr = sh_trampo_alloc(&sh_exit_trampo_mgr, pc, range_low, range_high);
  if (0 == addr) return -1;

  memcpy((void *)addr, exit, exit_len);
  sh_util_clear_cache(addr, exit_len);
  *exit_addr = addr;
  return 0;
}

static void sh_exit_free_out_library(uintptr_t exit_addr) {
  sh_trampo_free(&sh_exit_trampo_mgr, exit_addr);
}

// in-library mode:
//
// We store the shellcode for exit in the memory gaps in the ELF.

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct {
  uintptr_t start;
  uintptr_t end;
  bool need_fill_zero;
  bool readable;
} sh_exit_gap_t;
#pragma clang diagnostic pop

static size_t sh_exit_get_gaps(xdl_info_t *dlinfo, sh_exit_gap_t *gaps, size_t gaps_cap,
                               bool elf_loaded_by_kernel) {
  size_t gaps_used = 0;

  for (size_t i = 0; i < dlinfo->dlpi_phnum; i++) {
    // current LOAD segment
    const ElfW(Phdr) *cur_phdr = &(dlinfo->dlpi_phdr[i]);
    if (PT_LOAD != cur_phdr->p_type) continue;

    // next LOAD segment
    const ElfW(Phdr) *next_phdr = NULL;
    if (!elf_loaded_by_kernel) {
      for (size_t j = i + 1; j < dlinfo->dlpi_phnum; j++) {
        if (PT_LOAD == dlinfo->dlpi_phdr[j].p_type) {
          next_phdr = &(dlinfo->dlpi_phdr[j]);
          break;
        }
      }
    }

    uintptr_t cur_end = (uintptr_t)dlinfo->dli_fbase + cur_phdr->p_vaddr + cur_phdr->p_memsz;
    uintptr_t cur_page_end = SH_UTIL_PAGE_END(cur_end);
    uintptr_t cur_file_end = (uintptr_t)dlinfo->dli_fbase + cur_phdr->p_vaddr + cur_phdr->p_filesz;
    uintptr_t cur_file_page_end = SH_UTIL_PAGE_END(cur_file_end);
    uintptr_t next_page_start =
        (NULL == next_phdr ? cur_page_end
                           : SH_UTIL_PAGE_START((uintptr_t)dlinfo->dli_fbase + next_phdr->p_vaddr));

    sh_exit_gap_t gap = {0, 0, false, false};
    if (cur_phdr->p_flags & PF_X) {
      // From: last PF_X page's unused memory tail space.
      // To: next page start.
      gap.start = SH_UTIL_ALIGN_END(cur_end, 0x10);
      gap.end = next_page_start;
      gap.need_fill_zero = true;
      gap.readable = (cur_phdr->p_flags & PF_R && cur_page_end == next_page_start);
    } else if (cur_page_end > cur_file_page_end) {
      // From: last .bss page(which must NOT be file backend)'s unused memory tail space.
      // To: next page start.
      gap.start = SH_UTIL_ALIGN_END(cur_end, 0x10);
      gap.end = next_page_start;
      gap.need_fill_zero = false;
      gap.readable = (cur_phdr->p_flags & PF_R && cur_page_end == next_page_start);
    } else if (next_page_start > cur_page_end) {
      // Entire unused memory pages.
      gap.start = cur_page_end;
      gap.end = next_page_start;
      gap.need_fill_zero = true;
      gap.readable = false;
    }

    if ((gap.need_fill_zero && gap.end > gap.start + 0x10) || (!gap.need_fill_zero && gap.end > gap.start)) {
      SH_LOG_INFO("exit: gap, %" PRIxPTR " - %" PRIxPTR " (load_bias %" PRIxPTR ", %" PRIxPTR " - %" PRIxPTR
                  "), NFZ %d, READABLE %d",
                  gap.start, gap.end, (uintptr_t)dlinfo->dli_fbase, gap.start - (uintptr_t)dlinfo->dli_fbase,
                  gap.end - (uintptr_t)dlinfo->dli_fbase, gap.need_fill_zero ? 1 : 0, gap.readable ? 1 : 0);
      gaps[gaps_used].start = gap.start;
      gaps[gaps_used].end = gap.end;
      gaps[gaps_used].need_fill_zero = gap.need_fill_zero;
      gaps[gaps_used].readable = gap.readable;
      gaps_used++;
    }

    if (gaps_used >= gaps_cap) break;
  }

  return gaps_used;
}

static bool sh_exit_is_in_elf_range(uintptr_t pc, sh_exit_elfinfo_t *info) {
  if (pc < info->load_bias) return false;

  uintptr_t vaddr = pc - info->load_bias;
  for (size_t i = 0; i < info->dlpi_phnum; i++) {
    const ElfW(Phdr) *phdr = &(info->dlpi_phdr[i]);
    if (PT_LOAD != phdr->p_type) continue;

    if (phdr->p_vaddr <= vaddr && vaddr < phdr->p_vaddr + phdr->p_memsz) return true;
  }

  return false;
}

static bool sh_exit_is_elf_loaded_by_kernel(uintptr_t pc) {
  if (NULL == sh_exit_app_process_info.dlpi_phdr) return true;
  if (sh_exit_is_in_elf_range(pc, &sh_exit_app_process_info)) return true;

  if (NULL == sh_exit_linker_info.dlpi_phdr) return true;
  if (sh_exit_is_in_elf_range(pc, &sh_exit_linker_info)) return true;

  // vdso may not exist
  if (NULL != sh_exit_vdso_info.dlpi_phdr)
    if (sh_exit_is_in_elf_range(pc, &sh_exit_vdso_info)) return true;

  return false;
}

static bool sh_exit_is_zero(uintptr_t buf, size_t buf_len) {
  for (uintptr_t i = buf; i < buf + buf_len; i += sizeof(uintptr_t))
    if (*((uintptr_t *)i) != 0) return false;

  return true;
}

static int sh_exit_fill_zero(uintptr_t start, uintptr_t end, bool readable, xdl_info_t *dlinfo) {
  size_t size = end - start;
  bool set_prot_rwx = false;

  if (!readable) {
    if (0 != sh_util_mprotect(start, size, PROT_READ | PROT_WRITE | PROT_EXEC)) return -1;
    set_prot_rwx = true;
  }

  if (*((uint64_t *)start) != SH_EXIT_CLEAR_FLAG) {
    SH_LOG_INFO("exit: gap fill zero, %" PRIxPTR " - %" PRIxPTR " (load_bias %" PRIxPTR ", %" PRIxPTR
                " - %" PRIxPTR "), READABLE %d",
                start, end, (uintptr_t)dlinfo->dli_fbase, start - (uintptr_t)dlinfo->dli_fbase,
                end - (uintptr_t)dlinfo->dli_fbase, readable ? 1 : 0);
    if (!set_prot_rwx)
      if (0 != sh_util_mprotect(start, size, PROT_READ | PROT_WRITE | PROT_EXEC)) return -1;
    memset((void *)start, 0, size);
    *((uint64_t *)(start)) = SH_EXIT_CLEAR_FLAG;
    sh_util_clear_cache(start, size);
  }

  return 0;
}

static int sh_exit_try_alloc_in_library(uintptr_t *exit_addr, uintptr_t pc, xdl_info_t *dlinfo, uint8_t *exit,
                                        size_t exit_len, size_t range_low, size_t range_high, uintptr_t start,
                                        uintptr_t end) {
  if (pc >= range_low) start = SH_UTIL_MAX(start, pc - range_low);
  start = SH_UTIL_ALIGN_END(start, exit_len);

  if (range_high <= UINTPTR_MAX - pc) end = SH_UTIL_MIN(end - exit_len, pc + range_high);
  end = SH_UTIL_ALIGN_START(end, exit_len);

  if (end < start) return -1;
  SH_LOG_INFO("exit: gap resize, %" PRIxPTR " - %" PRIxPTR " (load_bias %" PRIxPTR ", %" PRIxPTR
              " - %" PRIxPTR ")",
              start, end, (uintptr_t)dlinfo->dli_fbase, start - (uintptr_t)dlinfo->dli_fbase,
              end - (uintptr_t)dlinfo->dli_fbase);

  for (uintptr_t cur = start; cur <= end; cur += exit_len) {
    // check if the current space has been used
    if (!sh_exit_is_zero(cur, exit_len)) continue;

    // write shellcode to the current location
    if (0 != sh_util_mprotect(cur, exit_len, PROT_READ | PROT_WRITE | PROT_EXEC)) return -1;
    memcpy((void *)cur, exit, exit_len);
    sh_util_clear_cache(cur, exit_len);
    *exit_addr = cur;
    SH_LOG_INFO("exit: in-library alloc, at %" PRIxPTR " (load_bias %" PRIxPTR ", %" PRIxPTR "), len %zu",
                cur, (uintptr_t)dlinfo->dli_fbase, cur - (uintptr_t)dlinfo->dli_fbase, exit_len);
    return 0;
  }
  return -1;
}

static int sh_exit_alloc_in_library(uintptr_t *exit_addr, uintptr_t pc, xdl_info_t *dlinfo, uint8_t *exit,
                                    size_t exit_len, size_t range_low, size_t range_high) {
  int r = -1;
  *exit_addr = 0;

  bool elf_loaded_by_kernel = sh_exit_is_elf_loaded_by_kernel(pc);

  pthread_mutex_lock(&sh_exit_lock);

  SH_SIG_TRY(SIGSEGV, SIGBUS) {
    sh_exit_gap_t gaps[SH_EXIT_GAPS_CAP];
    size_t gaps_used = sh_exit_get_gaps(dlinfo, gaps, SH_EXIT_GAPS_CAP, elf_loaded_by_kernel);
    for (size_t i = 0; i < gaps_used; i++) {
      // fill zero
      if (gaps[i].need_fill_zero) {
        if (0 != sh_exit_fill_zero(gaps[i].start, gaps[i].end, gaps[i].readable, dlinfo)) return -1;
      }

      if (0 == (r = sh_exit_try_alloc_in_library(exit_addr, pc, dlinfo, exit, exit_len, range_low, range_high,
                                                 gaps[i].start, gaps[i].end)))
        break;
    }
  }
  SH_SIG_CATCH() {
    r = -1;
    *exit_addr = 0;
    SH_LOG_WARN("exit: alloc crashed");
  }
  SH_SIG_EXIT

  pthread_mutex_unlock(&sh_exit_lock);
  return r;
}

static int sh_exit_free_in_library(uintptr_t exit_addr, uint8_t *exit, size_t exit_len) {
  int r;

  pthread_mutex_lock(&sh_exit_lock);

  SH_SIG_TRY(SIGSEGV, SIGBUS) {
    if (0 != memcmp((void *)exit_addr, exit, exit_len)) {
      r = SHADOWHOOK_ERRNO_UNHOOK_EXIT_MISMATCH;
      goto err;
    }
    if (0 != sh_util_mprotect((uintptr_t)exit_addr, exit_len, PROT_READ | PROT_WRITE | PROT_EXEC)) {
      r = SHADOWHOOK_ERRNO_MPROT;
      goto err;
    }
    memset((void *)exit_addr, 0, exit_len);
    sh_util_clear_cache((uintptr_t)exit_addr, exit_len);
    r = 0;
  err:;
  }
  SH_SIG_CATCH() {
    r = SHADOWHOOK_ERRNO_UNHOOK_EXIT_CRASH;
    SH_LOG_WARN("exit: free crashed");
  }
  SH_SIG_EXIT

  pthread_mutex_unlock(&sh_exit_lock);
  return r;
}

int sh_exit_alloc(uintptr_t *exit_addr, uint16_t *exit_type, uintptr_t pc, xdl_info_t *dlinfo, uint8_t *exit,
                  size_t exit_len, size_t range_low, size_t range_high) {
  int r;

  // (1) try out-library mode first. Because ELF gaps are a valuable non-renewable resource.
  *exit_type = SH_EXIT_TYPE_OUT_LIBRARY;
  r = sh_exit_alloc_out_library(exit_addr, pc, dlinfo, exit, exit_len, range_low, range_high);
  if (0 == r) goto ok;

  // (2) try in-library mode.
  *exit_type = SH_EXIT_TYPE_IN_LIBRARY;
  r = sh_exit_alloc_in_library(exit_addr, pc, dlinfo, exit, exit_len, range_low, range_high);
  if (0 == r) goto ok;

  return r;

ok:
  SH_LOG_INFO("exit: alloc %s library, exit %" PRIxPTR ", pc %" PRIxPTR ", distance %" PRIxPTR
              ", range [-%zx, %zx]",
              (*exit_type == SH_EXIT_TYPE_OUT_LIBRARY ? "out" : "in"), *exit_addr, pc,
              (pc > *exit_addr ? pc - *exit_addr : *exit_addr - pc), range_low, range_high);
  return 0;
}

int sh_exit_free(uintptr_t exit_addr, uint16_t exit_type, uint8_t *exit, size_t exit_len) {
  if (SH_EXIT_TYPE_OUT_LIBRARY == exit_type) {
    (void)exit, (void)exit_len;
    sh_exit_free_out_library(exit_addr);
    return 0;
  } else
    return sh_exit_free_in_library(exit_addr, exit, exit_len);
}

#pragma clang diagnostic pop
