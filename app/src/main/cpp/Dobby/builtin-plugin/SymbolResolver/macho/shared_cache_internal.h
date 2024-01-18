#include <stddef.h>
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

#if __i386__
#define ARCH_NAME "i386"
#define ARCH_CACHE_MAGIC "dyld_v1    i386"
#elif __x86_64__
#define ARCH_NAME "x86_64"
#define ARCH_CACHE_MAGIC "dyld_v1  x86_64"
#define ARCH_NAME_H "x86_64h"
#define ARCH_CACHE_MAGIC_H "dyld_v1 x86_64h"
#elif __ARM_ARCH_7K__
#define ARCH_NAME "armv7k"
#define ARCH_CACHE_MAGIC "dyld_v1  armv7k"
#elif __ARM_ARCH_7A__
#define ARCH_NAME "armv7"
#define ARCH_CACHE_MAGIC "dyld_v1   armv7"
#elif __ARM_ARCH_7S__
#define ARCH_NAME "armv7s"
#define ARCH_CACHE_MAGIC "dyld_v1  armv7s"
#elif __arm64e__
#define ARCH_NAME "arm64e"
#define ARCH_CACHE_MAGIC "dyld_v1  arm64e"
#elif __arm64__
#if __LP64__
#define ARCH_NAME "arm64"
#define ARCH_CACHE_MAGIC "dyld_v1   arm64"
#else
#define ARCH_NAME "arm64_32"
#define ARCH_CACHE_MAGIC "dyld_v1arm64_32"
#endif
#endif

typedef uintptr_t addr_t;

typedef struct shared_cache_ctx {
  struct dyld_cache_header *runtime_shared_cache;
  struct dyld_cache_header *mmap_shared_cache;

  uintptr_t runtime_slide;

  struct dyld_cache_local_symbols_info *local_symbols_info;
  struct dyld_cache_local_symbols_entry *local_symbols_entries;

  nlist_t *symtab;
  char *strtab;

} shared_cache_ctx_t;

int shared_cache_ctx_init(shared_cache_ctx_t *ctx);

bool shared_cache_is_contain(shared_cache_ctx_t *ctx, addr_t addr, size_t length);

int shared_cache_get_symbol_table(shared_cache_ctx_t *ctx, mach_header_t *image_header, nlist_t **out_symtab,
                                  uint32_t *out_symtab_count, char **out_strtab);
