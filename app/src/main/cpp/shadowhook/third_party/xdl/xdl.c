// Copyright (c) 2020-2023 HexHacking Team
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

// Created by caikelun on 2020-10-04.

#include "xdl.h"

#include <android/api-level.h>
#include <elf.h>
#include <fcntl.h>
#include <inttypes.h>
#include <link.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "xdl_iterate.h"
#include "xdl_linker.h"
#include "xdl_lzma.h"
#include "xdl_util.h"

#ifndef __LP64__
#define XDL_LIB_PATH "/system/lib"
#else
#define XDL_LIB_PATH "/system/lib64"
#endif

#define XDL_DYNSYM_IS_EXPORT_SYM(shndx) (SHN_UNDEF != (shndx))
#define XDL_SYMTAB_IS_EXPORT_SYM(shndx) \
  (SHN_UNDEF != (shndx) && !((shndx) >= SHN_LORESERVE && (shndx) <= SHN_HIRESERVE))

extern __attribute((weak)) unsigned long int getauxval(unsigned long int);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"

typedef struct xdl {
  char *pathname;
  uintptr_t load_bias;
  const ElfW(Phdr) *dlpi_phdr;
  ElfW(Half) dlpi_phnum;

  struct xdl *next;     // to next xdl obj for cache in xdl_addr()
  void *linker_handle;  // hold handle returned by xdl_linker_force_dlopen()

  //
  // (1) for searching symbols from .dynsym
  //

  bool dynsym_try_load;
  ElfW(Sym) *dynsym;   // .dynsym
  const char *dynstr;  // .dynstr

  // .hash (SYSV hash for .dynstr)
  struct {
    const uint32_t *buckets;
    uint32_t buckets_cnt;
    const uint32_t *chains;
    uint32_t chains_cnt;
  } sysv_hash;

  // .gnu.hash (GNU hash for .dynstr)
  struct {
    const uint32_t *buckets;
    uint32_t buckets_cnt;
    const uint32_t *chains;
    uint32_t symoffset;
    const ElfW(Addr) *bloom;
    uint32_t bloom_cnt;
    uint32_t bloom_shift;
  } gnu_hash;

  //
  // (2) for searching symbols from .symtab
  //

  bool symtab_try_load;
  uintptr_t base;

  ElfW(Sym) *symtab;  // .symtab
  size_t symtab_cnt;
  char *strtab;  // .strtab
  size_t strtab_sz;
} xdl_t;

#pragma clang diagnostic pop

// load from memory
static int xdl_dynsym_load(xdl_t *self) {
  // find the dynamic segment
  ElfW(Dyn) *dynamic = NULL;
  for (size_t i = 0; i < self->dlpi_phnum; i++) {
    const ElfW(Phdr) *phdr = &(self->dlpi_phdr[i]);
    if (PT_DYNAMIC == phdr->p_type) {
      dynamic = (ElfW(Dyn) *)(self->load_bias + phdr->p_vaddr);
      break;
    }
  }
  if (NULL == dynamic) return -1;

  // iterate the dynamic segment
  for (ElfW(Dyn) *entry = dynamic; entry && entry->d_tag != DT_NULL; entry++) {
    switch (entry->d_tag) {
      case DT_SYMTAB:  //.dynsym
        self->dynsym = (ElfW(Sym) *)(self->load_bias + entry->d_un.d_ptr);
        break;
      case DT_STRTAB:  //.dynstr
        self->dynstr = (const char *)(self->load_bias + entry->d_un.d_ptr);
        break;
      case DT_HASH:  //.hash
        self->sysv_hash.buckets_cnt = ((const uint32_t *)(self->load_bias + entry->d_un.d_ptr))[0];
        self->sysv_hash.chains_cnt = ((const uint32_t *)(self->load_bias + entry->d_un.d_ptr))[1];
        self->sysv_hash.buckets = &(((const uint32_t *)(self->load_bias + entry->d_un.d_ptr))[2]);
        self->sysv_hash.chains = &(self->sysv_hash.buckets[self->sysv_hash.buckets_cnt]);
        break;
      case DT_GNU_HASH:  //.gnu.hash
        self->gnu_hash.buckets_cnt = ((const uint32_t *)(self->load_bias + entry->d_un.d_ptr))[0];
        self->gnu_hash.symoffset = ((const uint32_t *)(self->load_bias + entry->d_un.d_ptr))[1];
        self->gnu_hash.bloom_cnt = ((const uint32_t *)(self->load_bias + entry->d_un.d_ptr))[2];
        self->gnu_hash.bloom_shift = ((const uint32_t *)(self->load_bias + entry->d_un.d_ptr))[3];
        self->gnu_hash.bloom = (const ElfW(Addr) *)(self->load_bias + entry->d_un.d_ptr + 16);
        self->gnu_hash.buckets = (const uint32_t *)(&(self->gnu_hash.bloom[self->gnu_hash.bloom_cnt]));
        self->gnu_hash.chains = (const uint32_t *)(&(self->gnu_hash.buckets[self->gnu_hash.buckets_cnt]));
        break;
      default:
        break;
    }
  }

  if (NULL == self->dynsym || NULL == self->dynstr ||
      (0 == self->sysv_hash.buckets_cnt && 0 == self->gnu_hash.buckets_cnt)) {
    self->dynsym = NULL;
    self->dynstr = NULL;
    self->sysv_hash.buckets_cnt = 0;
    self->gnu_hash.buckets_cnt = 0;
    return -1;
  }

  return 0;
}

static void *xdl_read_file_to_heap(int file_fd, size_t file_sz, size_t data_offset, size_t data_len) {
  if (0 == data_len) return NULL;
  if (data_offset >= file_sz) return NULL;
  if (data_offset + data_len > file_sz) return NULL;

  if (data_offset != (size_t)lseek(file_fd, (off_t)data_offset, SEEK_SET)) return NULL;

  void *data = malloc(data_len);
  if (NULL == data) return NULL;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-statement-expression"
  if ((ssize_t)data_len != XDL_UTIL_TEMP_FAILURE_RETRY(read(file_fd, data, data_len)))
#pragma clang diagnostic pop
  {
    free(data);
    return NULL;
  }

  return data;
}

static void *xdl_read_file_to_heap_by_section(int file_fd, size_t file_sz, ElfW(Shdr) *shdr) {
  return xdl_read_file_to_heap(file_fd, file_sz, (size_t)shdr->sh_offset, shdr->sh_size);
}

static void *xdl_read_memory_to_heap(void *mem, size_t mem_sz, size_t data_offset, size_t data_len) {
  if (0 == data_len) return NULL;
  if (data_offset >= mem_sz) return NULL;
  if (data_offset + data_len > mem_sz) return NULL;

  void *data = malloc(data_len);
  if (NULL == data) return NULL;

  memcpy(data, (void *)((uintptr_t)mem + data_offset), data_len);
  return data;
}

static void *xdl_read_memory_to_heap_by_section(void *mem, size_t mem_sz, ElfW(Shdr) *shdr) {
  return xdl_read_memory_to_heap(mem, mem_sz, (size_t)shdr->sh_offset, shdr->sh_size);
}

static void *xdl_get_memory(void *mem, size_t mem_sz, size_t data_offset, size_t data_len) {
  if (0 == data_len) return NULL;
  if (data_offset >= mem_sz) return NULL;
  if (data_offset + data_len > mem_sz) return NULL;

  return (void *)((uintptr_t)mem + data_offset);
}

static void *xdl_get_memory_by_section(void *mem, size_t mem_sz, ElfW(Shdr) *shdr) {
  return xdl_get_memory(mem, mem_sz, (size_t)shdr->sh_offset, shdr->sh_size);
}

// load from disk and memory
static int xdl_symtab_load_from_debugdata(xdl_t *self, int file_fd, size_t file_sz,
                                          ElfW(Shdr) *shdr_debugdata) {
  void *debugdata = NULL;
  ElfW(Shdr) *shdrs = NULL;
  int r = -1;

  // get zipped .gnu_debugdata
  uint8_t *debugdata_zip = (uint8_t *)xdl_read_file_to_heap_by_section(file_fd, file_sz, shdr_debugdata);
  if (NULL == debugdata_zip) return -1;

  // get unzipped .gnu_debugdata
  size_t debugdata_sz;
  if (0 != xdl_lzma_decompress(debugdata_zip, shdr_debugdata->sh_size, (uint8_t **)&debugdata, &debugdata_sz))
    goto end;

  // get ELF header
  ElfW(Ehdr) *ehdr = (ElfW(Ehdr) *)debugdata;
  if (0 == ehdr->e_shnum || ehdr->e_shentsize != sizeof(ElfW(Shdr))) goto end;

  // get section headers
  shdrs = (ElfW(Shdr) *)xdl_read_memory_to_heap(debugdata, debugdata_sz, (size_t)ehdr->e_shoff,
                                                ehdr->e_shentsize * ehdr->e_shnum);
  if (NULL == shdrs) goto end;

  // get .shstrtab
  if (SHN_UNDEF == ehdr->e_shstrndx || ehdr->e_shstrndx >= ehdr->e_shnum) goto end;
  char *shstrtab = (char *)xdl_get_memory_by_section(debugdata, debugdata_sz, shdrs + ehdr->e_shstrndx);
  if (NULL == shstrtab) goto end;

  // find .symtab & .strtab
  for (ElfW(Shdr) *shdr = shdrs; shdr < shdrs + ehdr->e_shnum; shdr++) {
    char *shdr_name = shstrtab + shdr->sh_name;

    if (SHT_SYMTAB == shdr->sh_type && 0 == strcmp(".symtab", shdr_name)) {
      // get & check associated .strtab section
      if (shdr->sh_link >= ehdr->e_shnum) continue;
      ElfW(Shdr) *shdr_strtab = shdrs + shdr->sh_link;
      if (SHT_STRTAB != shdr_strtab->sh_type) continue;

      // get .symtab & .strtab
      ElfW(Sym) *symtab = (ElfW(Sym) *)xdl_read_memory_to_heap_by_section(debugdata, debugdata_sz, shdr);
      if (NULL == symtab) continue;
      char *strtab = (char *)xdl_read_memory_to_heap_by_section(debugdata, debugdata_sz, shdr_strtab);
      if (NULL == strtab) {
        free(symtab);
        continue;
      }

      // OK
      self->symtab = symtab;
      self->symtab_cnt = shdr->sh_size / shdr->sh_entsize;
      self->strtab = strtab;
      self->strtab_sz = shdr_strtab->sh_size;
      r = 0;
      break;
    }
  }

end:
  free(debugdata_zip);
  if (NULL != debugdata) free(debugdata);
  if (NULL != shdrs) free(shdrs);
  return r;
}

// load from disk and memory
static int xdl_symtab_load(xdl_t *self) {
  if ('[' == self->pathname[0]) return -1;

  int r = -1;
  ElfW(Shdr) *shdrs = NULL;
  char *shstrtab = NULL;

  // get base address
  uintptr_t vaddr_min = UINTPTR_MAX;
  for (size_t i = 0; i < self->dlpi_phnum; i++) {
    const ElfW(Phdr) *phdr = &(self->dlpi_phdr[i]);
    if (PT_LOAD == phdr->p_type) {
      if (vaddr_min > phdr->p_vaddr) vaddr_min = phdr->p_vaddr;
    }
  }
  if (UINTPTR_MAX == vaddr_min) return -1;
  self->base = self->load_bias + vaddr_min;

  // open file
  int flags = O_RDONLY | O_CLOEXEC;
  int file_fd;
  if ('/' == self->pathname[0]) {
    file_fd = open(self->pathname, flags);
  } else {
    char full_pathname[1024];
    // try the fast method
    snprintf(full_pathname, sizeof(full_pathname), "%s/%s", XDL_LIB_PATH, self->pathname);
    file_fd = open(full_pathname, flags);
    if (file_fd < 0) {
      // try the slow method
      if (0 != xdl_iterate_get_full_pathname(self->base, full_pathname, sizeof(full_pathname))) return -1;
      file_fd = open(full_pathname, flags);
    }
  }
  if (file_fd < 0) return -1;
  struct stat st;
  if (0 != fstat(file_fd, &st)) goto end;
  size_t file_sz = (size_t)st.st_size;

  // get ELF header
  ElfW(Ehdr) *ehdr = (ElfW(Ehdr) *)self->base;
  if (0 == ehdr->e_shnum || ehdr->e_shentsize != sizeof(ElfW(Shdr))) goto end;

  // get section headers
  shdrs = (ElfW(Shdr) *)xdl_read_file_to_heap(file_fd, file_sz, (size_t)ehdr->e_shoff,
                                              ehdr->e_shentsize * ehdr->e_shnum);
  if (NULL == shdrs) goto end;

  // get .shstrtab
  if (SHN_UNDEF == ehdr->e_shstrndx || ehdr->e_shstrndx >= ehdr->e_shnum) goto end;
  shstrtab = (char *)xdl_read_file_to_heap_by_section(file_fd, file_sz, shdrs + ehdr->e_shstrndx);
  if (NULL == shstrtab) goto end;

  // find .symtab & .strtab
  for (ElfW(Shdr) *shdr = shdrs; shdr < shdrs + ehdr->e_shnum; shdr++) {
    char *shdr_name = shstrtab + shdr->sh_name;

    if (SHT_SYMTAB == shdr->sh_type && 0 == strcmp(".symtab", shdr_name)) {
      // get & check associated .strtab section
      if (shdr->sh_link >= ehdr->e_shnum) continue;
      ElfW(Shdr) *shdr_strtab = shdrs + shdr->sh_link;
      if (SHT_STRTAB != shdr_strtab->sh_type) continue;

      // get .symtab & .strtab
      ElfW(Sym) *symtab = (ElfW(Sym) *)xdl_read_file_to_heap_by_section(file_fd, file_sz, shdr);
      if (NULL == symtab) continue;
      char *strtab = (char *)xdl_read_file_to_heap_by_section(file_fd, file_sz, shdr_strtab);
      if (NULL == strtab) {
        free(symtab);
        continue;
      }

      // OK
      self->symtab = symtab;
      self->symtab_cnt = shdr->sh_size / shdr->sh_entsize;
      self->strtab = strtab;
      self->strtab_sz = shdr_strtab->sh_size;
      r = 0;
      break;
    } else if (SHT_PROGBITS == shdr->sh_type && 0 == strcmp(".gnu_debugdata", shdr_name)) {
      if (0 == xdl_symtab_load_from_debugdata(self, file_fd, file_sz, shdr)) {
        // OK
        r = 0;
        break;
      }
    }
  }

end:
  close(file_fd);
  if (NULL != shdrs) free(shdrs);
  if (NULL != shstrtab) free(shstrtab);
  return r;
}

static xdl_t *xdl_find_from_auxv(unsigned long type, const char *pathname) {
  if (NULL == getauxval) return NULL;  // API level < 18

  uintptr_t val = (uintptr_t)getauxval(type);
  if (0 == val) return NULL;

  // get base
  uintptr_t base = (AT_PHDR == type ? (val & (~0xffful)) : val);
  if (0 != memcmp((void *)base, ELFMAG, SELFMAG)) return NULL;

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

  // create xDL object
  xdl_t *self;
  if (NULL == (self = calloc(1, sizeof(xdl_t)))) return NULL;
  if (NULL == (self->pathname = strdup(pathname))) {
    free(self);
    return NULL;
  }
  self->load_bias = load_bias;
  self->dlpi_phdr = dlpi_phdr;
  self->dlpi_phnum = dlpi_phnum;
  self->dynsym_try_load = false;
  self->symtab_try_load = false;
  return self;
}

static int xdl_find_iterate_cb(struct dl_phdr_info *info, size_t size, void *arg) {
  (void)size;

  uintptr_t *pkg = (uintptr_t *)arg;
  xdl_t **self = (xdl_t **)*pkg++;
  const char *filename = (const char *)*pkg;

  // check load_bias
  if (0 == info->dlpi_addr || NULL == info->dlpi_name) return 0;

  // check pathname
  if ('[' == filename[0]) {
    if (0 != strcmp(info->dlpi_name, filename)) return 0;
  } else if ('/' == filename[0]) {
    if ('/' == info->dlpi_name[0]) {
      if (0 != strcmp(info->dlpi_name, filename)) return 0;
    } else {
      if (!xdl_util_ends_with(filename, info->dlpi_name)) return 0;
    }
  } else {
    if ('/' == info->dlpi_name[0]) {
      if (!xdl_util_ends_with(info->dlpi_name, filename)) return 0;
    } else {
      if (0 != strcmp(info->dlpi_name, filename)) return 0;
    }
  }

  // found the target ELF
  if (NULL == ((*self) = calloc(1, sizeof(xdl_t)))) return 1;  // return failed
  if (NULL == ((*self)->pathname = strdup(info->dlpi_name))) {
    free(*self);
    *self = NULL;
    return 1;  // return failed
  }
  (*self)->load_bias = info->dlpi_addr;
  (*self)->dlpi_phdr = info->dlpi_phdr;
  (*self)->dlpi_phnum = info->dlpi_phnum;
  (*self)->dynsym_try_load = false;
  (*self)->symtab_try_load = false;
  return 1;  // return OK
}

static xdl_t *xdl_find(const char *filename) {
  // from auxv (linker, vDSO)
  xdl_t *self = NULL;
  if (xdl_util_ends_with(filename, XDL_UTIL_LINKER_BASENAME))
    self = xdl_find_from_auxv(AT_BASE, XDL_UTIL_LINKER_PATHNAME);
  else if (xdl_util_ends_with(filename, XDL_UTIL_VDSO_BASENAME))
    self = xdl_find_from_auxv(AT_SYSINFO_EHDR, XDL_UTIL_VDSO_BASENAME);

  // from auxv (app_process)
  const char *basename, *pathname;
#if (defined(__arm__) || defined(__i386__)) && __ANDROID_API__ < __ANDROID_API_L__
  if (xdl_util_get_api_level() < __ANDROID_API_L__) {
    basename = XDL_UTIL_APP_PROCESS_BASENAME_K;
    pathname = XDL_UTIL_APP_PROCESS_PATHNAME_K;
  } else
#endif
  {
    basename = XDL_UTIL_APP_PROCESS_BASENAME;
    pathname = XDL_UTIL_APP_PROCESS_PATHNAME;
  }
  if (xdl_util_ends_with(filename, basename)) self = xdl_find_from_auxv(AT_PHDR, pathname);

  if (NULL != self) return self;

  // from dl_iterate_phdr
  uintptr_t pkg[2] = {(uintptr_t)&self, (uintptr_t)filename};
  xdl_iterate_phdr(xdl_find_iterate_cb, pkg, XDL_DEFAULT);
  return self;
}

static void *xdl_open_always_force(const char *filename) {
  // always force dlopen()
  void *linker_handle = xdl_linker_force_dlopen(filename);
  if (NULL == linker_handle) return NULL;

  // find
  xdl_t *self = xdl_find(filename);
  if (NULL == self)
    dlclose(linker_handle);
  else
    self->linker_handle = linker_handle;

  return (void *)self;
}

static void *xdl_open_try_force(const char *filename) {
  // find
  xdl_t *self = xdl_find(filename);
  if (NULL != self) return (void *)self;

  // try force dlopen()
  void *linker_handle = xdl_linker_force_dlopen(filename);
  if (NULL == linker_handle) return NULL;

  // find again
  self = xdl_find(filename);
  if (NULL == self)
    dlclose(linker_handle);
  else
    self->linker_handle = linker_handle;

  return (void *)self;
}

void *xdl_open(const char *filename, int flags) {
  if (NULL == filename) return NULL;

  if (flags & XDL_ALWAYS_FORCE_LOAD)
    return xdl_open_always_force(filename);
  else if (flags & XDL_TRY_FORCE_LOAD)
    return xdl_open_try_force(filename);
  else
    return xdl_find(filename);
}

void *xdl_close(void *handle) {
  if (NULL == handle) return NULL;

  xdl_t *self = (xdl_t *)handle;
  if (NULL != self->pathname) free(self->pathname);
  if (NULL != self->symtab) free(self->symtab);
  if (NULL != self->strtab) free(self->strtab);

  void *linker_handle = self->linker_handle;
  free(self);
  return linker_handle;
}

static uint32_t xdl_sysv_hash(const uint8_t *name) {
  uint32_t h = 0, g;

  while (*name) {
    h = (h << 4) + *name++;
    g = h & 0xf0000000;
    h ^= g;
    h ^= g >> 24;
  }
  return h;
}

static uint32_t xdl_gnu_hash(const uint8_t *name) {
  uint32_t h = 5381;

  while (*name) {
    h += (h << 5) + *name++;
  }
  return h;
}

static ElfW(Sym) *xdl_dynsym_find_symbol_use_sysv_hash(xdl_t *self, const char *sym_name) {
  uint32_t hash = xdl_sysv_hash((const uint8_t *)sym_name);

  for (uint32_t i = self->sysv_hash.buckets[hash % self->sysv_hash.buckets_cnt]; 0 != i;
       i = self->sysv_hash.chains[i]) {
    ElfW(Sym) *sym = self->dynsym + i;
    if (0 != strcmp(self->dynstr + sym->st_name, sym_name)) continue;
    return sym;
  }

  return NULL;
}

static ElfW(Sym) *xdl_dynsym_find_symbol_use_gnu_hash(xdl_t *self, const char *sym_name) {
  uint32_t hash = xdl_gnu_hash((const uint8_t *)sym_name);

  static uint32_t elfclass_bits = sizeof(ElfW(Addr)) * 8;
  size_t word = self->gnu_hash.bloom[(hash / elfclass_bits) % self->gnu_hash.bloom_cnt];
  size_t mask = 0 | (size_t)1 << (hash % elfclass_bits) |
                (size_t)1 << ((hash >> self->gnu_hash.bloom_shift) % elfclass_bits);

  // if at least one bit is not set, this symbol is surely missing
  if ((word & mask) != mask) return NULL;

  // ignore STN_UNDEF
  uint32_t i = self->gnu_hash.buckets[hash % self->gnu_hash.buckets_cnt];
  if (i < self->gnu_hash.symoffset) return NULL;

  // loop through the chain
  while (1) {
    ElfW(Sym) *sym = self->dynsym + i;
    uint32_t sym_hash = self->gnu_hash.chains[i - self->gnu_hash.symoffset];

    if ((hash | (uint32_t)1) == (sym_hash | (uint32_t)1)) {
      if (0 == strcmp(self->dynstr + sym->st_name, sym_name)) {
        return sym;
      }
    }

    // chain ends with an element with the lowest bit set to 1
    if (sym_hash & (uint32_t)1) break;

    i++;
  }

  return NULL;
}

void *xdl_sym(void *handle, const char *symbol, size_t *symbol_size) {
  if (NULL == handle || NULL == symbol) return NULL;
  if (NULL != symbol_size) *symbol_size = 0;

  xdl_t *self = (xdl_t *)handle;

  // load .dynsym only once
  if (!self->dynsym_try_load) {
    self->dynsym_try_load = true;
    if (0 != xdl_dynsym_load(self)) return NULL;
  }

  // find symbol
  if (NULL == self->dynsym) return NULL;
  ElfW(Sym) *sym = NULL;
  if (self->gnu_hash.buckets_cnt > 0) {
    // use GNU hash (.gnu.hash -> .dynsym -> .dynstr), O(x) + O(1) + O(1)
    sym = xdl_dynsym_find_symbol_use_gnu_hash(self, symbol);
  }
  if (NULL == sym && self->sysv_hash.buckets_cnt > 0) {
    // use SYSV hash (.hash -> .dynsym -> .dynstr), O(x) + O(1) + O(1)
    sym = xdl_dynsym_find_symbol_use_sysv_hash(self, symbol);
  }
  if (NULL == sym || !XDL_DYNSYM_IS_EXPORT_SYM(sym->st_shndx)) return NULL;

  if (NULL != symbol_size) *symbol_size = sym->st_size;
  return (void *)(self->load_bias + sym->st_value);
}

// clang-format off
/*
 * For internal symbols in .symtab, LLVM may add some suffixes (for example for thinLTO).
 * The format of the suffix is: ".xxxx.[hash]". LLVM may add multiple suffixes at once.
 * The symbol name after removing these all suffixes is called canonical name.
 *
 * Because the hash part in the suffix may change when recompiled, so here we only match
 * the canonical name.
 *
 * IN ADDITION: According to C/C++ syntax, it is illegal for a function name to contain
 * dot character('.'), either in the middle or at the end.
 *
 * samples:
 *
 * symbol name in .symtab          lookup                       is match
 * ----------------------          ----------------             --------
 * abcd                            abc                          N
 * abcd                            abcd                         Y
 * abcd.llvm.10190306339727611508  abc                          N
 * abcd.llvm.10190306339727611508  abcd                         Y
 * abcd.llvm.10190306339727611508  abcd.                        N
 * abcd.llvm.10190306339727611508  abcd.llvm                    Y
 * abcd.llvm.10190306339727611508  abcd.llvm.                   N
 * abcd.__uniq.513291356003753     abcd.__uniq.51329            N
 * abcd.__uniq.513291356003753     abcd.__uniq.513291356003753  Y
 */
// clang-format on
static inline bool xdl_dsym_is_match(const char *str, const char *sym, size_t str_len) {
  if (__predict_false(0 == str_len)) return false;

  do {
    if (*str != *sym) return __predict_false('.' == *str && '\0' == *sym);
    str++;
    sym++;
    if ('\0' == *str) break;
  } while (0 != --str_len);

  return true;
}

void *xdl_dsym(void *handle, const char *symbol, size_t *symbol_size) {
  if (NULL == handle || NULL == symbol) return NULL;
  if (NULL != symbol_size) *symbol_size = 0;

  xdl_t *self = (xdl_t *)handle;

  // load .symtab only once
  if (!self->symtab_try_load) {
    self->symtab_try_load = true;
    if (0 != xdl_symtab_load(self)) return NULL;
  }

  // find symbol
  if (NULL == self->symtab) return NULL;
  for (size_t i = 0; i < self->symtab_cnt; i++) {
    ElfW(Sym) *sym = self->symtab + i;

    if (!XDL_SYMTAB_IS_EXPORT_SYM(sym->st_shndx)) continue;
    // if (0 != strncmp(self->strtab + sym->st_name, symbol, self->strtab_sz - sym->st_name)) continue;
    if (!xdl_dsym_is_match(self->strtab + sym->st_name, symbol, self->strtab_sz - sym->st_name)) continue;

    if (NULL != symbol_size) *symbol_size = sym->st_size;
    return (void *)(self->load_bias + sym->st_value);
  }

  return NULL;
}

static bool xdl_elf_is_match(uintptr_t load_bias, const ElfW(Phdr) *dlpi_phdr, ElfW(Half) dlpi_phnum,
                             uintptr_t addr) {
  if (addr < load_bias) return false;

  uintptr_t vaddr = addr - load_bias;
  for (size_t i = 0; i < dlpi_phnum; i++) {
    const ElfW(Phdr) *phdr = &(dlpi_phdr[i]);
    if (PT_LOAD != phdr->p_type) continue;

    if (phdr->p_vaddr <= vaddr && vaddr < phdr->p_vaddr + phdr->p_memsz) return true;
  }

  return false;
}

static int xdl_open_by_addr_iterate_cb(struct dl_phdr_info *info, size_t size, void *arg) {
  (void)size;

  uintptr_t *pkg = (uintptr_t *)arg;
  xdl_t **self = (xdl_t **)*pkg++;
  uintptr_t addr = *pkg;

  if (xdl_elf_is_match(info->dlpi_addr, info->dlpi_phdr, info->dlpi_phnum, addr)) {
    // found the target ELF
    if (NULL == ((*self) = calloc(1, sizeof(xdl_t)))) return 1;  // failed
    if (NULL == ((*self)->pathname = strdup(info->dlpi_name))) {
      free(*self);
      *self = NULL;
      return 1;  // failed
    }
    (*self)->load_bias = info->dlpi_addr;
    (*self)->dlpi_phdr = info->dlpi_phdr;
    (*self)->dlpi_phnum = info->dlpi_phnum;
    (*self)->dynsym_try_load = false;
    (*self)->symtab_try_load = false;
    return 1;  // OK
  }

  return 0;  // mismatch
}

static void *xdl_open_by_addr(void *addr) {
  if (NULL == addr) return NULL;

  xdl_t *self = NULL;
  uintptr_t pkg[2] = {(uintptr_t)&self, (uintptr_t)addr};
  xdl_iterate_phdr(xdl_open_by_addr_iterate_cb, pkg, XDL_DEFAULT);

  return (void *)self;
}

static bool xdl_sym_is_match(ElfW(Sym) *sym, uintptr_t offset, bool is_symtab) {
  if (is_symtab) {
    if (!XDL_SYMTAB_IS_EXPORT_SYM(sym->st_shndx)) false;
  } else {
    if (!XDL_DYNSYM_IS_EXPORT_SYM(sym->st_shndx)) false;
  }

  return ELF_ST_TYPE(sym->st_info) != STT_TLS && offset >= sym->st_value &&
         offset < sym->st_value + sym->st_size;
}

static ElfW(Sym) *xdl_sym_by_addr(void *handle, void *addr) {
  xdl_t *self = (xdl_t *)handle;

  // load .dynsym only once
  if (!self->dynsym_try_load) {
    self->dynsym_try_load = true;
    if (0 != xdl_dynsym_load(self)) return NULL;
  }

  // find symbol
  if (NULL == self->dynsym) return NULL;
  uintptr_t offset = (uintptr_t)addr - self->load_bias;
  if (self->gnu_hash.buckets_cnt > 0) {
    const uint32_t *chains_all = self->gnu_hash.chains - self->gnu_hash.symoffset;
    for (size_t i = 0; i < self->gnu_hash.buckets_cnt; i++) {
      uint32_t n = self->gnu_hash.buckets[i];
      if (n < self->gnu_hash.symoffset) continue;
      do {
        ElfW(Sym) *sym = self->dynsym + n;
        if (xdl_sym_is_match(sym, offset, false)) return sym;
      } while ((chains_all[n++] & 1) == 0);
    }
  } else if (self->sysv_hash.chains_cnt > 0) {
    for (size_t i = 0; i < self->sysv_hash.chains_cnt; i++) {
      ElfW(Sym) *sym = self->dynsym + i;
      if (xdl_sym_is_match(sym, offset, false)) return sym;
    }
  }

  return NULL;
}

static ElfW(Sym) *xdl_dsym_by_addr(void *handle, void *addr) {
  xdl_t *self = (xdl_t *)handle;

  // load .symtab only once
  if (!self->symtab_try_load) {
    self->symtab_try_load = true;
    if (0 != xdl_symtab_load(self)) return NULL;
  }

  // find symbol
  if (NULL == self->symtab) return NULL;
  uintptr_t offset = (uintptr_t)addr - self->load_bias;
  for (size_t i = 0; i < self->symtab_cnt; i++) {
    ElfW(Sym) *sym = self->symtab + i;
    if (xdl_sym_is_match(sym, offset, true)) return sym;
  }

  return NULL;
}

int xdl_addr(void *addr, xdl_info_t *info, void **cache) {
  if (NULL == addr || NULL == info || NULL == cache) return 0;

  memset(info, 0, sizeof(Dl_info));

  // find handle from cache
  xdl_t *handle = NULL;
  for (handle = *((xdl_t **)cache); NULL != handle; handle = handle->next)
    if (xdl_elf_is_match(handle->load_bias, handle->dlpi_phdr, handle->dlpi_phnum, (uintptr_t)addr)) break;

  // create new handle, save handle to cache
  if (NULL == handle) {
    handle = (xdl_t *)xdl_open_by_addr(addr);
    if (NULL == handle) return 0;
    handle->next = *(xdl_t **)cache;
    *(xdl_t **)cache = handle;
  }

  // we have at least: load_bias, pathname, dlpi_phdr, dlpi_phnum
  info->dli_fbase = (void *)handle->load_bias;
  info->dli_fname = handle->pathname;
  info->dli_sname = NULL;
  info->dli_saddr = 0;
  info->dli_ssize = 0;
  info->dlpi_phdr = handle->dlpi_phdr;
  info->dlpi_phnum = (size_t)handle->dlpi_phnum;

  // keep looking for: symbol name, symbol offset, symbol size
  ElfW(Sym) *sym;
  if (NULL != (sym = xdl_sym_by_addr((void *)handle, addr))) {
    info->dli_sname = handle->dynstr + sym->st_name;
    info->dli_saddr = (void *)(handle->load_bias + sym->st_value);
    info->dli_ssize = sym->st_size;
  } else if (NULL != (sym = xdl_dsym_by_addr((void *)handle, addr))) {
    info->dli_sname = handle->strtab + sym->st_name;
    info->dli_saddr = (void *)(handle->load_bias + sym->st_value);
    info->dli_ssize = sym->st_size;
  }

  return 1;
}

void xdl_addr_clean(void **cache) {
  if (NULL == cache) return;

  xdl_t *handle = *((xdl_t **)cache);
  while (NULL != handle) {
    xdl_t *tmp = handle;
    handle = handle->next;
    xdl_close(tmp);
  }
  *cache = NULL;
}

int xdl_iterate_phdr(int (*callback)(struct dl_phdr_info *, size_t, void *), void *data, int flags) {
  if (NULL == callback) return 0;

  return xdl_iterate_phdr_impl(callback, data, flags);
}

int xdl_info(void *handle, int request, void *info) {
  if (NULL == handle || XDL_DI_DLINFO != request || NULL == info) return -1;

  xdl_t *self = (xdl_t *)handle;
  xdl_info_t *dlinfo = (xdl_info_t *)info;

  dlinfo->dli_fbase = (void *)self->load_bias;
  dlinfo->dli_fname = self->pathname;
  dlinfo->dli_sname = NULL;
  dlinfo->dli_saddr = 0;
  dlinfo->dli_ssize = 0;
  dlinfo->dlpi_phdr = self->dlpi_phdr;
  dlinfo->dlpi_phnum = (size_t)self->dlpi_phnum;
  return 0;
}
