#include "macho_file_symbol_resolver.h"

#include "SymbolResolver/mmap_file_util.h"

#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

uintptr_t macho_file_memory_symbol_resolve(cpu_type_t in_cputype, cpu_subtype_t in_cpusubtype, const uint8_t *file_mem,
                                           char *symbol_name_pattern) {

  mach_header_t *header = (mach_header_t *)file_mem;
  struct fat_header *fh = (struct fat_header *)file_mem;
  if (fh->magic == OSSwapBigToHostInt32(FAT_MAGIC)) {
    const struct fat_arch *archs = (struct fat_arch *)(((uintptr_t)fh) + sizeof(fat_header));
    mach_header_t *header_arm64 = NULL;
    mach_header_t *header_arm64e = NULL;
    mach_header_t *header_x64 = NULL;
    for (size_t i = 0; i < OSSwapBigToHostInt32(fh->nfat_arch); i++) {
      uint64_t offset;
      uint64_t len;
      cpu_type_t cputype = (cpu_type_t)OSSwapBigToHostInt32(archs[i].cputype);
      cpu_subtype_t cpusubtype = (cpu_subtype_t)OSSwapBigToHostInt32(archs[i].cpusubtype);
      offset = OSSwapBigToHostInt32(archs[i].offset);
      len = OSSwapBigToHostInt32(archs[i].size);
      if (cputype == CPU_TYPE_X86_64) {
        header_x64 = (mach_header_t *)&file_mem[offset];
      } else if (cputype == CPU_TYPE_ARM64 && (cpusubtype & ~CPU_SUBTYPE_MASK) == CPU_SUBTYPE_ARM64E) {
        header_arm64e = (mach_header_t *)&file_mem[offset];
      } else if (cputype == CPU_TYPE_ARM64) {
        header_arm64 = (mach_header_t *)&file_mem[offset];
      }

      if ((cputype == in_cputype) && ((cpusubtype & in_cpusubtype) == in_cpusubtype)) {
        header = (mach_header_t *)&file_mem[offset];
        break;
      }
    }

    if (header == (mach_header_t *)file_mem) {
      if (in_cputype == 0 && in_cpusubtype == 0) {
#if defined(__arm64__) || defined(__aarch64__)
        header = header_arm64e ? header_arm64e : header_arm64;
#else
        header = header_x64;
#endif
      }
    }
  }

  macho_ctx_t ctx(header, false);
  return ctx.symbol_resolve_options(symbol_name_pattern, RESOLVE_SYMBOL_TYPE_SYMBOL_TABLE);
}

uintptr_t macho_file_symbol_resolve(cpu_type_t cpu, cpu_subtype_t subtype, const char *file,
                                    char *symbol_name_pattern) {

#if defined(COMPILE_WITH_NO_STDLIB)
  return 0;
#endif
  MmapFileManager mng(file);
  auto mmap_buffer = mng.map();
  if (!mmap_buffer) {
    return 0;
  }

  return macho_file_memory_symbol_resolve(cpu, subtype, mmap_buffer, symbol_name_pattern);
}
