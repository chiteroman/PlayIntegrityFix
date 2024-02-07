#pragma once

#include <sys/types.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

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

intptr_t read_sleb128(const uint8_t **pp, const uint8_t *end);

uintptr_t read_uleb128(const uint8_t **pp, const uint8_t *end);

typedef enum {
  RESOLVE_SYMBOL_TYPE_SYMBOL_TABLE = 1 << 0,
  RESOLVE_SYMBOL_TYPE_EXPORTED = 1 << 1,
  RESOLVE_SYMBOL_TYPE_ALL = RESOLVE_SYMBOL_TYPE_SYMBOL_TABLE | RESOLVE_SYMBOL_TYPE_EXPORTED
} resolve_symbol_type_t;

struct macho_ctx_t {
  bool is_runtime_mode;

  mach_header_t *header;

  uintptr_t vmaddr;
  size_t vmsize;
  uintptr_t vm_region_start;
  uintptr_t vm_region_end;

  uintptr_t slide;
  uintptr_t linkedit_base;

  segment_command_t *segments[64];
  int segments_count;

  segment_command_t *text_seg;
  segment_command_t *data_seg;
  segment_command_t *text_exec_seg;
  segment_command_t *data_const_seg;
  segment_command_t *linkedit_seg;

  struct symtab_command *symtab_cmd;
  struct dysymtab_command *dysymtab_cmd;
  struct dyld_info_command *dyld_info_cmd;
  struct linkedit_data_command *exports_trie_cmd;
  struct linkedit_data_command *chained_fixups_cmd;

  nlist_t *symtab;
  char *strtab;
  uint32_t *indirect_symtab;

  explicit macho_ctx_t(mach_header_t *header, bool is_runtime_mode = true) {
    init(header, is_runtime_mode);
  }

  void init(mach_header_t *header, bool is_runtime_mode);

  uintptr_t iterate_symbol_table(const char *symbol_name_pattern);

  uintptr_t iterate_exported_symbol(const char *symbol_name, uint64_t *out_flags);

  uintptr_t symbol_resolve_options(const char *symbol_name_pattern, resolve_symbol_type_t type);

  uintptr_t symbol_resolve(const char *symbol_name_pattern);
};

#ifdef __cplusplus
extern "C" {
#endif

uintptr_t macho_iterate_symbol_table(char *name_pattern, nlist_t *symtab, uint32_t symtab_count, char *strtab);

#ifdef __cplusplus
}
#endif
