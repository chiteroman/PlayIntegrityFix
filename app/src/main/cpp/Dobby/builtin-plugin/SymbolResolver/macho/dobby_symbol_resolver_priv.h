#include <stdint.h>
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

typedef struct macho_ctx {
  mach_header_t *header;

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

  nlist_t *symtab;
  char *strtab;
  uint32_t *indirect_symtab;

} macho_ctx_t;

void macho_ctx_init(macho_ctx_t *ctx, mach_header_t *header);

uintptr_t iterate_symbol_table(char *name_pattern, nlist_t *symtab, uint32_t symtab_count, char *strtab);
