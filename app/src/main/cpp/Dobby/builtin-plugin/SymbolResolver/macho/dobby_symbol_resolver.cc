#include "dobby_symbol_resolver.h"
#include "macho/dobby_symbol_resolver_priv.h"
#include "macho_file_symbol_resolver.h"

#include "dobby/common.h"

#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#include "PlatformUtil/ProcessRuntimeUtility.h"

#include "macho_ctx.h"
#include "shared_cache_ctx.h"

#if !defined(BUILDING_KERNEL)
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#endif

#undef LOG_TAG
#define LOG_TAG "DobbySymbolResolver"

PUBLIC void *DobbySymbolResolver(const char *image_name, const char *symbol_name_pattern) {
  uintptr_t result = 0;
  auto modules = ProcessRuntimeUtility::GetProcessModuleMap();

  for (auto iter = modules.begin(); iter != modules.end(); iter++) {
    auto module = *iter;

    // image filter
    if (image_name && !strstr(module.path, image_name))
      continue;

    // dyld in shared cached at new os version
    // ignore dyld, as some functions as own implementation in dyld
    if (!image_name && strstr(module.path, "dyld"))
      continue;

    auto header = (mach_header_t *)module.load_address;
    if (header == nullptr)
      continue;

#if 0
    DEBUG_LOG("resolve image: %s", module.path);
#endif

    nlist_t *symtab = NULL;
    uint32_t symtab_count = 0;
    char *strtab = NULL;

#if !defined(BUILDING_KERNEL)
#if defined(__arm__) || defined(__aarch64__)
    static int shared_cache_ctx_init_once = 0;
    static shared_cache_ctx_t shared_cache_ctx;
    if (shared_cache_ctx_init_once == 0) {
      shared_cache_ctx_init_once = 1;
      shared_cache_ctx_init(&shared_cache_ctx);
      shared_cache_load_symbols(&shared_cache_ctx);
    }
    if (shared_cache_ctx.mmap_shared_cache) {
      // shared cache library
      if (shared_cache_is_contain(&shared_cache_ctx, (addr_t)header, 0)) {
        shared_cache_get_symbol_table(&shared_cache_ctx, header, &symtab, &symtab_count, &strtab);
      }
    }
    if (symtab && strtab) {
      result = macho_iterate_symbol_table((char *)symbol_name_pattern, symtab, symtab_count, strtab);
    }
    if (result) {
      result = result + shared_cache_ctx.runtime_slide;
      return (void *)result;
    }
#endif
#endif

    macho_ctx_t macho_ctx(header);
    result = macho_ctx.symbol_resolve(symbol_name_pattern);
    if (result) {
      return (void *)result;
    }
  }

#if !defined(BUILDING_KERNEL)
  mach_header_t *dyld_header = NULL;
  if (image_name != NULL && strcmp(image_name, "dyld") == 0) {
    // task info
    task_dyld_info_data_t task_dyld_info;
    mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_DYLD_INFO, (task_info_t)&task_dyld_info, &count)) {
      return NULL;
    }

    // get dyld load address
    const struct dyld_all_image_infos *infos =
        (struct dyld_all_image_infos *)(uintptr_t)task_dyld_info.all_image_info_addr;
    dyld_header = (mach_header_t *)infos->dyldImageLoadAddress;
    macho_ctx_t dyld_ctx(dyld_header);
    result = dyld_ctx.symbol_resolve(symbol_name_pattern);

    bool is_dyld_in_cache = ((mach_header_t *)dyld_header)->flags & MH_DYLIB_IN_CACHE;
    if (!is_dyld_in_cache && result == 0) {
      result = macho_file_symbol_resolve(dyld_header->cputype, dyld_header->cpusubtype, "/usr/lib/dyld",
                                         (char *)symbol_name_pattern);
      result += (uintptr_t)dyld_header;
    }
  }
#endif

  if (result == 0) {
    DEBUG_LOG("symbol resolver failed: %s", symbol_name_pattern);
  }

  return (void *)result;
}
