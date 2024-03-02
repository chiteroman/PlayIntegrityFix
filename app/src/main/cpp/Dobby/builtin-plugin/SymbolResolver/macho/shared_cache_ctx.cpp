#include "shared_cache_ctx.h"

#include <mach/mach.h>
#include <mach/task.h>
#include <mach-o/dyld_images.h>

#include "logging/logging.h"

#include "mmap_file_util.h"

typedef uintptr_t addr_t;

extern "C" {
extern const char *dyld_shared_cache_file_path();
extern int __shared_region_check_np(uint64_t *startaddress);
}

const char *shared_cache_get_file_path() {
  return dyld_shared_cache_file_path();
}

struct dyld_cache_header *shared_cache_get_load_addr() {
  addr_t shared_cache_base = 0;
  if (__shared_region_check_np((uint64_t *)&shared_cache_base) != 0) {
    WARN_LOG("__shared_region_check_np failed");
  }

  if (shared_cache_base)
    return (struct dyld_cache_header *)shared_cache_base;

  // task info
  task_dyld_info_data_t task_dyld_info;
  mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
  kern_return_t ret = task_info(mach_task_self(), TASK_DYLD_INFO, (task_info_t)&task_dyld_info, &count);
  if (ret != KERN_SUCCESS) {
    ERROR_LOG("task_info failed, ret: %d", ret);
    return NULL;
  }

  // get dyld load address
  auto *infos = (struct dyld_all_image_infos *)(uintptr_t)task_dyld_info.all_image_info_addr;
  auto *shared_cache = (struct dyld_cache_header *)infos->sharedCacheBaseAddress;
  return shared_cache;
}

int shared_cache_load_symbols(shared_cache_ctx_t *ctx) {
  uint64_t localSymbolsOffset = 0;

  bool latest_shared_cache_format = true;

  const char *shared_cache_path = shared_cache_get_file_path();
  char shared_cache_symbols_path[4096] = {0};
  {
    strcat(shared_cache_symbols_path, shared_cache_path);
    strcat(shared_cache_symbols_path, ".symbols");
  }

  auto mmapSharedCacheSymbolsMng = new MmapFileManager(shared_cache_symbols_path);
  auto mmap_buffer = mmapSharedCacheSymbolsMng->map();
  if (mmap_buffer) { // iphoneos >= 15.0, which has .symbols file
    ctx->mmap_shared_cache = (struct dyld_cache_header *)mmap_buffer;

    localSymbolsOffset = ctx->mmap_shared_cache->localSymbolsOffset;
  } else {
    // iphoneos < 15.0, which has no .symbols file
    auto mmapSharedCacheMng = new MmapFileManager(shared_cache_symbols_path);

    auto runtime_shared_cache = ctx->runtime_shared_cache;
    uint64_t mmap_length = runtime_shared_cache->localSymbolsSize;
    uint64_t mmap_offset = runtime_shared_cache->localSymbolsOffset;

    if (mmap_length == 0)
      return -1;

    auto mmap_buffer = mmapSharedCacheMng->map_options(mmap_length, mmap_offset);
    if (!mmap_buffer) {
      return -1;
    }

    // fake shared cache header
    auto mmap_shared_cache =
        (struct dyld_cache_header *)((addr_t)mmap_buffer - runtime_shared_cache->localSymbolsOffset);
    ctx->mmap_shared_cache = mmap_shared_cache;

    localSymbolsOffset = runtime_shared_cache->localSymbolsOffset;

    latest_shared_cache_format = false;
  }
  ctx->latest_shared_cache_format = latest_shared_cache_format;

  {
    auto mmap_shared_cache = ctx->mmap_shared_cache;
    auto localInfo = (struct dyld_cache_local_symbols_info *)((char *)mmap_shared_cache + localSymbolsOffset);
    ctx->local_symbols_info = localInfo;

    if (ctx->latest_shared_cache_format) {
      auto localEntries_64 = (struct dyld_cache_local_symbols_entry_64 *)((char *)localInfo + localInfo->entriesOffset);
      ctx->local_symbols_entries_64 = localEntries_64;
    } else {
      auto localEntries = (struct dyld_cache_local_symbols_entry *)((char *)localInfo + localInfo->entriesOffset);
      ctx->local_symbols_entries = localEntries;
    }

    ctx->symtab = (nlist_t *)((char *)localInfo + localInfo->nlistOffset);
    ctx->strtab = ((char *)localInfo) + localInfo->stringsOffset;
  }

  return 0;
}

int shared_cache_ctx_init(shared_cache_ctx_t *ctx) {
  memset(ctx, 0, sizeof(shared_cache_ctx_t));

  auto runtime_shared_cache = shared_cache_get_load_addr();
  if (!runtime_shared_cache) {
    return -1;
  }
  ctx->runtime_shared_cache = runtime_shared_cache;

  // shared cache slide
  auto mappings =
      (struct dyld_cache_mapping_info *)((char *)runtime_shared_cache + runtime_shared_cache->mappingOffset);
  uintptr_t slide = (uintptr_t)runtime_shared_cache - (uintptr_t)(mappings[0].address);
  ctx->runtime_slide = slide;

  return 0;
}

// refer: dyld
bool shared_cache_is_contain(shared_cache_ctx_t *ctx, addr_t addr, size_t length) {
  struct dyld_cache_header *runtime_shared_cache;
  if (ctx) {
    runtime_shared_cache = ctx->runtime_shared_cache;
  } else {
    runtime_shared_cache = shared_cache_get_load_addr();
  }

  addr_t region_start = runtime_shared_cache->sharedRegionStart + ctx->runtime_slide;
  addr_t region_end = region_start + runtime_shared_cache->sharedRegionSize;
  if (addr >= region_start && addr < region_end)
    return true;

  return false;
}

int shared_cache_get_symbol_table(shared_cache_ctx_t *ctx, mach_header_t *image_header, nlist_t **out_symtab,
                                  uint32_t *out_symtab_count, char **out_strtab) {
  uint64_t textOffsetInCache = (uint64_t)image_header - (uint64_t)ctx->runtime_shared_cache;

  nlist_t *localNlists = NULL;
  uint32_t localNlistCount = 0;
  const char *localStrings = NULL;

  const uint32_t entriesCount = ctx->local_symbols_info->entriesCount;
  for (uint32_t i = 0; i < entriesCount; ++i) {
    if (ctx->latest_shared_cache_format) {
      if (ctx->local_symbols_entries_64[i].dylibOffset == textOffsetInCache) {
        uint32_t localNlistStart = ctx->local_symbols_entries_64[i].nlistStartIndex;
        localNlistCount = ctx->local_symbols_entries_64[i].nlistCount;
        localNlists = &ctx->symtab[localNlistStart];
        break;
      }
    } else {
      if (ctx->local_symbols_entries[i].dylibOffset == textOffsetInCache) {
        uint32_t localNlistStart = ctx->local_symbols_entries[i].nlistStartIndex;
        localNlistCount = ctx->local_symbols_entries[i].nlistCount;
        localNlists = &ctx->symtab[localNlistStart];
        break;
      }
    }

#if 0
      static struct dyld_cache_image_info *imageInfos = NULL;
      imageInfos = (struct dyld_cache_image_info *)((addr_t)g_mmap_shared_cache + g_mmap_shared_cache->imagesOffset);
      char *image_name = (char *)g_mmap_shared_cache + imageInfos[i].pathFileOffset;
      INFO_LOG("dyld image: %s", image_name);
#endif
  }
  *out_symtab = localNlists;
  *out_symtab_count = (uint32_t)localNlistCount;
  *out_strtab = (char *)ctx->strtab;
  return 0;
}