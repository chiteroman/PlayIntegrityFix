#include "dobby_import_replace.h"

#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/dyld_images.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <mach/vm_map.h>
#include <mach/mach.h>
#include <sys/mman.h>

#include <vector>

#include "dobby/common.h"

#include "logging/logging.h"

#include "PlatformUtil/ProcessRuntimeUtility.h"

#if defined(__LP64__)
typedef struct mach_header_64 mach_header_t;
typedef struct segment_command_64 segment_command_t;
typedef struct section_64 section_t;
typedef struct nlist_64 nlist_t;
#define LC_SEGMENT_ARCH_DEPENDENT LC_SEGMENT_64
#else
typedef struct mach_header mach_header_t;
typedef struct segment_command segment_command_t;
typedef struct section section_t;
typedef struct nlist nlist_t;
#define LC_SEGMENT_ARCH_DEPENDENT LC_SEGMENT
#endif

static void *iterate_indirect_symtab(char *symbol_name, section_t *section, intptr_t slide, nlist_t *symtab,
                                     char *strtab, uint32_t *indirect_symtab) {
  const bool is_data_const = strcmp(section->segname, "__DATA_CONST") == 0;
  uint32_t *indirect_symbol_indices = indirect_symtab + section->reserved1;
  void **indirect_symbol_bindings = (void **)((uintptr_t)slide + section->addr);

  vm_prot_t old_protection = VM_PROT_READ;
  if (is_data_const) {
    mprotect(indirect_symbol_bindings, section->size, PROT_READ | PROT_WRITE);
  }

  for (uint i = 0; i < section->size / sizeof(void *); i++) {
    uint32_t symtab_index = indirect_symbol_indices[i];
    if (symtab_index == INDIRECT_SYMBOL_ABS || symtab_index == INDIRECT_SYMBOL_LOCAL ||
        symtab_index == (INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS)) {
      continue;
    }
    uint32_t strtab_offset = symtab[symtab_index].n_un.n_strx;
    char *local_symbol_name = strtab + strtab_offset;
    bool symbol_name_longer_than_1 = symbol_name[0] && symbol_name[1];
    if (strcmp(local_symbol_name, symbol_name) == 0) {
      return &indirect_symbol_bindings[i];
    }
    if (local_symbol_name[0] == '_') {
      if (strcmp(symbol_name, &local_symbol_name[1]) == 0) {
        return &indirect_symbol_bindings[i];
      }
    }
  }

  if (is_data_const && 0) {
    int protection = 0;
    if (old_protection & VM_PROT_READ) {
      protection |= PROT_READ;
    }
    if (old_protection & VM_PROT_WRITE) {
      protection |= PROT_WRITE;
    }
    if (old_protection & VM_PROT_EXECUTE) {
      protection |= PROT_EXEC;
    }
    mprotect(indirect_symbol_bindings, section->size, protection);
  }
  return NULL;
}

static void *get_global_offset_table_stub(mach_header_t *header, char *symbol_name) {
  segment_command_t *curr_seg_cmd;
  segment_command_t *text_segment, *data_segment, *linkedit_segment;
  struct symtab_command *symtab_cmd = NULL;
  struct dysymtab_command *dysymtab_cmd = NULL;

  uintptr_t cur = (uintptr_t)header + sizeof(mach_header_t);
  for (uint i = 0; i < header->ncmds; i++, cur += curr_seg_cmd->cmdsize) {
    curr_seg_cmd = (segment_command_t *)cur;
    if (curr_seg_cmd->cmd == LC_SEGMENT_ARCH_DEPENDENT) {
      if (strcmp(curr_seg_cmd->segname, "__LINKEDIT") == 0) {
        linkedit_segment = curr_seg_cmd;
      } else if (strcmp(curr_seg_cmd->segname, "__DATA") == 0) {
        data_segment = curr_seg_cmd;
      } else if (strcmp(curr_seg_cmd->segname, "__TEXT") == 0) {
        text_segment = curr_seg_cmd;
      }
    } else if (curr_seg_cmd->cmd == LC_SYMTAB) {
      symtab_cmd = (struct symtab_command *)curr_seg_cmd;
    } else if (curr_seg_cmd->cmd == LC_DYSYMTAB) {
      dysymtab_cmd = (struct dysymtab_command *)curr_seg_cmd;
    }
  }

  if (!symtab_cmd || !linkedit_segment || !linkedit_segment) {
    return NULL;
  }

  uintptr_t slide = (uintptr_t)header - (uintptr_t)text_segment->vmaddr;
  uintptr_t linkedit_base = (uintptr_t)slide + linkedit_segment->vmaddr - linkedit_segment->fileoff;
  nlist_t *symtab = (nlist_t *)(linkedit_base + symtab_cmd->symoff);
  char *strtab = (char *)(linkedit_base + symtab_cmd->stroff);
  uint32_t symtab_count = symtab_cmd->nsyms;

  uint32_t *indirect_symtab = (uint32_t *)(linkedit_base + dysymtab_cmd->indirectsymoff);

  cur = (uintptr_t)header + sizeof(mach_header_t);
  for (uint i = 0; i < header->ncmds; i++, cur += curr_seg_cmd->cmdsize) {
    curr_seg_cmd = (segment_command_t *)cur;
    if (curr_seg_cmd->cmd == LC_SEGMENT_ARCH_DEPENDENT) {
      if (strcmp(curr_seg_cmd->segname, "__DATA") != 0 && strcmp(curr_seg_cmd->segname, "__DATA_CONST") != 0) {
        continue;
      }
      for (uint j = 0; j < curr_seg_cmd->nsects; j++) {
        section_t *sect = (section_t *)(cur + sizeof(segment_command_t)) + j;
        if ((sect->flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS) {
          void *stub = iterate_indirect_symtab(symbol_name, sect, slide, symtab, strtab, indirect_symtab);
          if (stub)
            return stub;
        }
        if ((sect->flags & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS) {
          void *stub = iterate_indirect_symtab(symbol_name, sect, slide, symtab, strtab, indirect_symtab);
          if (stub)
            return stub;
        }
      }
    }
  }

  return NULL;
}

PUBLIC int DobbyImportTableReplace(char *image_name, char *symbol_name, void *fake_func, void **orig_func_ptr) {
  std::vector<RuntimeModule> ProcessModuleMap = ProcessRuntimeUtility::GetProcessModuleMap();

  for (auto module : ProcessModuleMap) {
    if (image_name != NULL && strstr(module.path, image_name) == NULL)
      continue;

    addr_t header = (addr_t)module.load_address;
    size_t slide = 0;

#if 0
    if (header) {
      if (((struct mach_header *)header)->magic == MH_MAGIC_64)
        slide = macho_kit_get_slide64(header);
    }
#endif

#if 0
    INFO_LOG("resolve image: %s", module.path);
#endif

    uint32_t nlist_count = 0;
    nlist_t *nlist_array = 0;
    char *string_pool = 0;

    void *stub = get_global_offset_table_stub((mach_header_t *)header, symbol_name);
    if (stub) {
      void *orig_func;
      orig_func = *(void **)stub;
#if __has_feature(ptrauth_calls)
      orig_func = ptrauth_strip(orig_func, ptrauth_key_asia);
      orig_func = ptrauth_sign_unauthenticated(orig_func, ptrauth_key_asia, 0);
#endif
      *orig_func_ptr = orig_func;

#if __has_feature(ptrauth_calls)
      fake_func = (void *)ptrauth_strip(fake_func, ptrauth_key_asia);
      fake_func = ptrauth_sign_unauthenticated(fake_func, ptrauth_key_asia, stub);
#endif
      *(void **)stub = fake_func;
    }

    if (image_name)
      return 0;
  }
  return -1;
}
