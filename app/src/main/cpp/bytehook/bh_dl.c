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

#include "bh_dl.h"

#include <android/api-level.h>
#include <elf.h>
#include <fcntl.h>
#include <inttypes.h>
#include <link.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "bh_const.h"
#include "bh_util.h"

extern __attribute((weak)) unsigned long int getauxval(unsigned long int);

#define BH_DL_SYMTAB_IS_EXPORT_SYM(shndx) \
  (SHN_UNDEF != (shndx) && !((shndx) >= SHN_LORESERVE && (shndx) <= SHN_HIRESERVE))

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
typedef struct {
  uintptr_t load_bias;
  uintptr_t base;
  ElfW(Sym) *symtab;  // .symtab
  size_t symtab_cnt;
  char *strtab;  // .strtab
  size_t strtab_sz;
} bh_dl_t;
#pragma clang diagnostic pop

static void *bh_dl_read_to_memory(int file_fd, size_t file_sz, size_t data_offset, size_t data_len) {
  if (0 == data_len) return NULL;
  if (data_offset + data_len > file_sz) return NULL;

  if (data_offset != (size_t)lseek(file_fd, (off_t)data_offset, SEEK_SET)) return NULL;

  void *data = malloc(data_len);
  if (NULL == data) return NULL;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"
  if ((ssize_t)data_len != BH_UTIL_TEMP_FAILURE_RETRY(read(file_fd, data, data_len)))
#pragma clang diagnostic pop
  {
    free(data);
    return NULL;
  }

  return data;
}

static void *bh_dl_read_section_to_memory(int file_fd, size_t file_sz, ElfW(Shdr) *shdr) {
  return bh_dl_read_to_memory(file_fd, file_sz, (size_t)shdr->sh_offset, shdr->sh_size);
}

static int bh_dl_load_symtab(bh_dl_t *self, const char *pathname) {
  ElfW(Shdr) *shdrs = NULL;
  char *shstrtab = NULL;

  // open file
  int file_fd = open(pathname, O_RDONLY | O_CLOEXEC);
  if (file_fd < 0) return -1;

  // get file size
  struct stat st;
  if (0 != fstat(file_fd, &st)) goto err;
  size_t file_sz = (size_t)st.st_size;

  // get ELF header from memory
  ElfW(Ehdr) *ehdr = (ElfW(Ehdr) *)self->base;
  if (0 == ehdr->e_shnum) goto err;

  // get section header table from file
  shdrs = (ElfW(Shdr) *)bh_dl_read_to_memory(file_fd, file_sz, (size_t)ehdr->e_shoff,
                                             ehdr->e_shentsize * ehdr->e_shnum);
  if (NULL == shdrs) goto err;

  // get .shstrtab from file
  ElfW(Shdr) *shdr_shstrtab = shdrs + ehdr->e_shstrndx;
  shstrtab = (char *)bh_dl_read_section_to_memory(file_fd, file_sz, shdr_shstrtab);
  if (NULL == shstrtab) goto err;

  // get .symtab and .strtab from file
  for (size_t i = 0; i < ehdr->e_shnum; i++) {
    ElfW(Shdr) *shdr = shdrs + i;
    char *shdr_name = shstrtab + shdr->sh_name;

    if (SHT_SYMTAB == shdr->sh_type && 0 == strcmp(".symtab", shdr_name)) {
      // check associated .strtab section info
      if (shdr->sh_link >= ehdr->e_shnum) continue;
      ElfW(Shdr) *shdr_strtab = shdrs + shdr->sh_link;
      if (SHT_STRTAB != shdr_strtab->sh_type) continue;

      // get .symtab from file
      self->symtab = (ElfW(Sym) *)bh_dl_read_section_to_memory(file_fd, file_sz, shdr);
      if (NULL == self->symtab) goto err;
      self->symtab_cnt = shdr->sh_size / shdr->sh_entsize;

      // get .strtab from file
      self->strtab = (char *)bh_dl_read_section_to_memory(file_fd, file_sz, shdr_strtab);
      if (NULL == self->strtab) goto err;
      self->strtab_sz = shdr_strtab->sh_size;

      close(file_fd);
      free(shdrs);
      free(shstrtab);
      return 0;  // OK
    }
  }

err:
  if (file_fd >= 0) close(file_fd);
  if (NULL != shdrs) free(shdrs);
  if (NULL != shstrtab) free(shstrtab);
  if (NULL != self->symtab) {
    free(self->symtab);
    self->symtab = NULL;
    self->symtab_cnt = 0;
  }
  if (NULL != self->strtab) {
    free(self->strtab);
    self->strtab = NULL;
    self->strtab_sz = 0;
  }
  return -1;  // not found
}

static uintptr_t bh_dl_find_linker_base_from_auxv(void) {
  if (NULL == getauxval) return 0;

  uintptr_t base = (uintptr_t)getauxval(AT_BASE);
  if (0 == base) return 0;
  if (0 != memcmp((void *)base, ELFMAG, SELFMAG)) return 0;
  return base;
}

#if __ANDROID_API__ < __ANDROID_API_J_MR2__
static uintptr_t bh_dl_find_linker_base_from_maps(void) {
  FILE *maps = fopen("/proc/self/maps", "r");
  if (NULL == maps) return 0;

  uintptr_t ret = 0;
  char line[1024];
  while (fgets(line, sizeof(line), maps)) {
    bh_util_trim_ending(line);
    if (!bh_util_ends_with(line, " " BH_CONST_BASENAME_LINKER) &&
        !bh_util_ends_with(line, "/" BH_CONST_BASENAME_LINKER))
      continue;

    uintptr_t base, offset;
    if (2 != sscanf(line, "%" SCNxPTR "-%*" SCNxPTR " r-xp %" SCNxPTR " ", &base, &offset)) break;
    if (0 != offset) break;
    if (0 != memcmp((void *)base, ELFMAG, SELFMAG)) break;

    ret = base;
    break;
  }

  fclose(maps);
  return ret;
}
#endif

void *bh_dl_open_linker(void) {
  uintptr_t base = bh_dl_find_linker_base_from_auxv();
#if __ANDROID_API__ < __ANDROID_API_J_MR2__
  if (0 == base) base = bh_dl_find_linker_base_from_maps();
#endif
  if (0 == base) return NULL;

  // ELF info
  ElfW(Ehdr) *ehdr = (ElfW(Ehdr) *)base;
  const ElfW(Phdr) *dlpi_phdr = (const ElfW(Phdr) *)(base + ehdr->e_phoff);
  ElfW(Half) dlpi_phnum = ehdr->e_phnum;

  // get bias
  uintptr_t min_vaddr = UINTPTR_MAX;
  for (size_t i = 0; i < dlpi_phnum; i++) {
    const ElfW(Phdr) *phdr = &(dlpi_phdr[i]);
    if (PT_LOAD == phdr->p_type) {
      if (min_vaddr > phdr->p_vaddr) min_vaddr = phdr->p_vaddr;
    }
  }
  if (UINTPTR_MAX == min_vaddr || base < min_vaddr) return NULL;
  uintptr_t load_bias = base - min_vaddr;

  // create bh_dl_t object
  bh_dl_t *self;
  if (NULL == (self = calloc(1, sizeof(bh_dl_t)))) return NULL;
  self->load_bias = load_bias;
  self->base = base;
  if (0 != bh_dl_load_symtab(self, BH_CONST_PATHNAME_LINKER)) {
    free(self);
    return NULL;
  }
  return (void *)self;
}

void bh_dl_close(void *handle) {
  bh_dl_t *self = (bh_dl_t *)handle;

  if (NULL != self->symtab) free(self->symtab);
  if (NULL != self->strtab) free(self->strtab);
  free(self);
}

void *bh_dl_dsym(void *handle, const char *symbol) {
  bh_dl_t *self = (bh_dl_t *)handle;

  for (size_t i = 0; i < self->symtab_cnt; i++) {
    ElfW(Sym) *sym = self->symtab + i;
    if (!BH_DL_SYMTAB_IS_EXPORT_SYM(sym->st_shndx)) continue;
    if (0 != strncmp(self->strtab + sym->st_name, symbol, self->strtab_sz - sym->st_name)) continue;
    return (void *)(self->load_bias + sym->st_value);
  }
  return NULL;
}
