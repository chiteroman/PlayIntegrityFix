#include <sys/types.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#include "shared-cache/dyld_cache_format.h"

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

typedef uintptr_t addr_t;

typedef struct shared_cache_ctx {
  struct dyld_cache_header *runtime_shared_cache;
  struct dyld_cache_header *mmap_shared_cache;

  uintptr_t runtime_slide;

  bool latest_shared_cache_format;
  struct dyld_cache_local_symbols_info *local_symbols_info;
  struct dyld_cache_local_symbols_entry *local_symbols_entries;
  struct dyld_cache_local_symbols_entry_64 *local_symbols_entries_64;

  nlist_t *symtab;
  char *strtab;
} shared_cache_ctx_t;

int shared_cache_ctx_init(shared_cache_ctx_t *ctx);

int shared_cache_load_symbols(shared_cache_ctx_t *ctx);

bool shared_cache_is_contain(shared_cache_ctx_t *ctx, addr_t addr, size_t length);

int shared_cache_get_symbol_table(shared_cache_ctx_t *ctx, mach_header_t *image_header, nlist_t **out_symtab,
                                  uint32_t *out_symtab_count, char **out_strtab);