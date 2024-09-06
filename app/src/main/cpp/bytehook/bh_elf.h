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
#include <elf.h>
#include <link.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "queue.h"
#include "tree.h"

// API level >= 21:
// https://android.googlesource.com/platform/bionic/+/refs/heads/master/linker/linker_common_types.h Android
// uses RELA for LP64.
#if defined(__LP64__)
typedef ElfW(Rela) Elf_Reloc;
#else
typedef ElfW(Rel) Elf_Reloc;
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"

// program header list
typedef struct bh_elf_ph {
  uintptr_t start;
  uintptr_t end;
  int protect;
  SLIST_ENTRY(bh_elf_ph, ) link;
} bh_elf_ph_t;
typedef SLIST_HEAD(bh_elf_ph_list, bh_elf_ph, ) bh_elf_ph_list_t;

// ELF info
typedef struct bh_elf {
  bool exist;
  pthread_mutex_t hook_lock;
  bool error;
#ifdef __LP64__
  bool cfi_hooked;
  bool cfi_hooked_ok;
  pthread_mutex_t cfi_hook_lock;
#endif

  const char *pathname;
  uintptr_t load_bias;
  const ElfW(Phdr) *dlpi_phdr;
  size_t dlpi_phnum;
  bool dyn_parsed;
  pthread_mutex_t dyn_parse_lock;

  const Elf_Reloc *rel_plt;  //.rel.plt / .rela.plt (relocation table for function)
  size_t rel_plt_cnt;

  const Elf_Reloc *rel_dyn;  //.rel.dyn / .rela.dyn (relocation table for data)
  size_t rel_dyn_cnt;

  uint8_t *rel_dyn_aps2;  //.rel.dyn / .rela.dyn (relocation table for data. APS2 format)
  size_t rel_dyn_aps2_sz;

  ElfW(Sym) *dynsym;   //.dynsym (dynamic symbol-table, symbol-index -> string-table's offset)
  const char *dynstr;  //.dynstr (dynamic string-table)

  //.hash (SYSV hash for string-table)
  struct {
    const uint32_t *buckets;
    uint32_t buckets_cnt;
    const uint32_t *chains;
    uint32_t chains_cnt;
  } sysv_hash;

  //.gnu.hash (GNU hash for string-table)
  struct {
    const uint32_t *buckets;
    uint32_t buckets_cnt;
    const uint32_t *chains;
    uint32_t symoffset;
    const ElfW(Addr) *bloom;
    uint32_t bloom_cnt;
    uint32_t bloom_shift;
  } gnu_hash;

  RB_ENTRY(bh_elf) link;
  TAILQ_ENTRY(bh_elf, ) link_list;
} bh_elf_t;
typedef TAILQ_HEAD(bh_elf_list, bh_elf, ) bh_elf_list_t;

#pragma clang diagnostic pop

bh_elf_t *bh_elf_create(struct dl_phdr_info *info);
void bh_elf_destroy(bh_elf_t **self);

bool bh_elf_is_match(bh_elf_t *self, const char *name);

bool bh_elf_get_error(bh_elf_t *self);
void bh_elf_set_error(bh_elf_t *self, bool error);

#ifdef __LP64__
void bh_elf_cfi_hook_lock(bh_elf_t *self);
void bh_elf_cfi_hook_unlock(bh_elf_t *self);
#endif

void bh_elf_hook_lock(bh_elf_t *self);
void bh_elf_hook_unlock(bh_elf_t *self);

void bh_elf_set_exist(bh_elf_t *self);
void bh_elf_unset_exist(bh_elf_t *self);
bool bh_elf_is_exist(bh_elf_t *self);

// get protect info by address
int bh_elf_get_protect_by_addr(bh_elf_t *self, void *addr);

// find export function symbol info by symbol name
// signal-safe
ElfW(Sym) *bh_elf_find_export_func_symbol_by_symbol_name(bh_elf_t *self, const char *sym_name);

// find import function address'address list by symbol name form .rel.plt and .rel.dyn
// signal-safe
size_t bh_elf_find_import_func_addr_by_symbol_name(bh_elf_t *self, const char *sym_name, void **addr_array,
                                                   size_t addr_array_cap);

// find import function address'address list by callee address form .rel.plt and .rel.dyn
// signal-safe
size_t bh_elf_find_import_func_addr_by_callee_addr(bh_elf_t *self, void *target_addr, void **addr_array,
                                                   size_t addr_array_cap);

// find export function address by symbol name (equal to dlsym())
// signal-safe
void *bh_elf_find_export_func_addr_by_symbol_name(bh_elf_t *self, const char *sym_name);
