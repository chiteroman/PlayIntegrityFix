#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <pthread.h>  // pthread_once
#include <sys/mman.h> // mmap
#include <fcntl.h>    // open

#include "SymbolResolver/macho/shared_cache_internal.h"
#include "SymbolResolver/macho/shared-cache/dyld_cache_format.h"

#include "logging/logging.h"

#undef LOG_TAG
#define LOG_TAG "DobbySymbolResolverCache"

#if 0
extern "C" {
int __shared_region_check_np(uint64_t *startaddress);
}
#endif

extern "C" const char *dyld_shared_cache_file_path();

static pthread_once_t mmap_dyld_shared_cache_once = PTHREAD_ONCE_INIT;

extern "C" int __shared_region_check_np(uint64_t *startaddress);

#include <sys/stat.h>

static char *fast_get_shared_cache_path() {
#if defined(_M_IX86) || defined(__i386__) || defined(_M_X64) || defined(__x86_64__)
  return NULL;
#endif
  char *result = NULL;
  char path_buffer[2048] = {0};

  const char *path = NULL;
  do {
    path = dyld_shared_cache_file_path();
    if (path != NULL) {
      break;
    } else {
      struct stat statbuf;
      int r = 0;

      path = IPHONE_DYLD_SHARED_CACHE_DIR DYLD_SHARED_CACHE_BASE_NAME "arm64";
      r = stat(path, &statbuf);
      if (r == 0) {
        break;
      }
      path = IPHONE_DYLD_SHARED_CACHE_DIR DYLD_SHARED_CACHE_BASE_NAME "arm64e";
      r = stat(path, &statbuf);
      if (r == 0) {
        break;
      }
      path = MACOSX_MRM_DYLD_SHARED_CACHE_DIR DYLD_SHARED_CACHE_BASE_NAME "arm64";
      r = stat(path, &statbuf);
      if (r == 0) {
        break;
      }
      path = MACOSX_MRM_DYLD_SHARED_CACHE_DIR DYLD_SHARED_CACHE_BASE_NAME "arm64e";
      r = stat(path, &statbuf);
      if (r == 0) {
        break;
      }
    }
  } while (0);

  if (path != NULL) {
    strcpy(path_buffer, path);
    result = (char *)malloc(strlen(path_buffer) + 1);
    strcpy(result, path_buffer);
  }

  return result;
}

#include <mach/mach.h>
#include <mach/task.h>
#include <mach-o/dyld_images.h>
struct dyld_cache_header *shared_cache_get_load_addr() {
  static struct dyld_cache_header *shared_cache_load_addr = 0;

  // task info
  task_dyld_info_data_t task_dyld_info;
  mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
  if (task_info(mach_task_self(), TASK_DYLD_INFO, (task_info_t)&task_dyld_info, &count)) {
    return NULL;
  }

  // get dyld load address
  const struct dyld_all_image_infos *infos =
      (struct dyld_all_image_infos *)(uintptr_t)task_dyld_info.all_image_info_addr;
  shared_cache_load_addr = (struct dyld_cache_header *)infos->sharedCacheBaseAddress;

  return shared_cache_load_addr;

#if 0
  if (shared_cache_load_addr)
    return shared_cache_load_addr;
#if 0
  if (syscall(294, &shared_cache_load_addr) == 0) {
#else
  if (__shared_region_check_np((uint64_t *)&shared_cache_load_addr) != 0) {
#endif
  shared_cache_load_addr = 0;
}
#endif
  return shared_cache_load_addr;
}

int shared_cache_ctx_init(shared_cache_ctx_t *ctx) {
  int fd;
  const char *cache_file_path = NULL;

  cache_file_path = fast_get_shared_cache_path();
  if (cache_file_path == NULL) {
    return -1;
  }

  fd = open(cache_file_path, O_RDONLY, 0);
  if (fd == -1) {
    return KERN_FAILURE;
  }

  struct dyld_cache_header *runtime_shared_cache;
  struct dyld_cache_header *mmap_shared_cache;

  // auto align
  runtime_shared_cache = shared_cache_get_load_addr();
  if (runtime_shared_cache == NULL) {
    return KERN_FAILURE;
  }

  // maybe shared cache is apple silicon
  if (runtime_shared_cache->localSymbolsSize == 0) {
    return KERN_FAILURE;
  }

  size_t mmap_length = runtime_shared_cache->localSymbolsSize;
  off_t mmap_offset = runtime_shared_cache->localSymbolsOffset;
  mmap_shared_cache =
      (struct dyld_cache_header *)mmap(0, mmap_length, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, mmap_offset);
  if (mmap_shared_cache == MAP_FAILED) {
    DLOG(0, "mmap shared cache failed");
    return KERN_FAILURE;
  }

  // fake shared cache header
  mmap_shared_cache =
      (struct dyld_cache_header *)((addr_t)mmap_shared_cache - runtime_shared_cache->localSymbolsOffset);

  ctx->runtime_shared_cache = runtime_shared_cache;
  ctx->mmap_shared_cache = mmap_shared_cache;

  // shared cache slide
  const struct dyld_cache_mapping_info *mappings =
      (struct dyld_cache_mapping_info *)((char *)runtime_shared_cache + runtime_shared_cache->mappingOffset);
  uintptr_t slide = (uintptr_t)runtime_shared_cache - (uintptr_t)(mappings[0].address);
  ctx->runtime_slide = slide;

  // shared cache symbol table
  static struct dyld_cache_local_symbols_info *localInfo = NULL;
  localInfo =
      (struct dyld_cache_local_symbols_info *)((char *)mmap_shared_cache + runtime_shared_cache->localSymbolsOffset);

  static struct dyld_cache_local_symbols_entry *localEntries = NULL;
  localEntries = (struct dyld_cache_local_symbols_entry *)((char *)localInfo + localInfo->entriesOffset);

  ctx->local_symbols_info = localInfo;
  ctx->local_symbols_entries = localEntries;

  ctx->symtab = (nlist_t *)((char *)localInfo + localInfo->nlistOffset);
  ctx->strtab = ((char *)localInfo) + localInfo->stringsOffset;
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

  const struct dyld_cache_mapping_info *mappings =
      (struct dyld_cache_mapping_info *)((char *)runtime_shared_cache + runtime_shared_cache->mappingOffset);
  uintptr_t slide = (uintptr_t)runtime_shared_cache - (uintptr_t)(mappings[0].address);
  uintptr_t unslidStart = (uintptr_t)addr - slide;

  // quick out if after end of cache
  if (unslidStart > (mappings[2].address + mappings[2].size))
    return false;

  // walk cache regions
  const struct dyld_cache_mapping_info *mappingsEnd = &mappings[runtime_shared_cache->mappingCount];
  uintptr_t unslidEnd = unslidStart + length;
  for (const struct dyld_cache_mapping_info *m = mappings; m < mappingsEnd; ++m) {
    if ((unslidStart >= m->address) && (unslidEnd < (m->address + m->size))) {
      return true;
    }
  }
  return false;
}

int shared_cache_get_symbol_table(shared_cache_ctx_t *ctx, mach_header_t *image_header, nlist_t **out_symtab,
                                  uint32_t *out_symtab_count, char **out_strtab) {
  struct dyld_cache_header *runtime_shared_cache = NULL;

  runtime_shared_cache = ctx->runtime_shared_cache;

  uint64_t textOffsetInCache = (uint64_t)image_header - (uint64_t)runtime_shared_cache;

  nlist_t *localNlists = NULL;
  uint32_t localNlistCount = 0;
  const char *localStrings = NULL;

  const uint32_t entriesCount = ctx->local_symbols_info->entriesCount;
  for (uint32_t i = 0; i < entriesCount; ++i) {
    if (ctx->local_symbols_entries[i].dylibOffset == textOffsetInCache) {
      uint32_t localNlistStart = ctx->local_symbols_entries[i].nlistStartIndex;
      localNlistCount = ctx->local_symbols_entries[i].nlistCount;
      localNlists = &ctx->symtab[localNlistStart];

#if 0
      static struct dyld_cache_image_info *imageInfos = NULL;
      imageInfos = (struct dyld_cache_image_info *)((addr_t)g_mmap_shared_cache + g_mmap_shared_cache->imagesOffset);
      char *image_name = (char *)g_mmap_shared_cache + imageInfos[i].pathFileOffset;
      LOG(1, "dyld image: %s", image_name);
#endif
    }
  }
  *out_symtab = localNlists;
  *out_symtab_count = (uint32_t)localNlistCount;
  *out_strtab = (char *)ctx->strtab;
  return 0;
}
