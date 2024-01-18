#include "PlatformUtil/ProcessRuntimeUtility.h"

#include <elf.h>
#include <dlfcn.h>
#include <link.h>
#include <sys/mman.h>

#include <string>
#include <string.h>

#include <vector>
#include <algorithm>

#define LINE_MAX 2048

// ================================================================
// GetProcessMemoryLayout

static bool memory_region_comparator(MemRange a, MemRange b) {
  return (a.start < b.start);
}

std::vector<MemRegion> regions;
const std::vector<MemRegion> &ProcessRuntimeUtility::GetProcessMemoryLayout() {
  regions.clear();

  FILE *fp = fopen("/proc/self/maps", "r");
  if (fp == nullptr)
    return regions;

  while (!feof(fp)) {
    char line_buffer[LINE_MAX + 1];
    fgets(line_buffer, LINE_MAX, fp);

    // ignore the rest of characters
    if (strlen(line_buffer) == LINE_MAX && line_buffer[LINE_MAX] != '\n') {
      // Entry not describing executable data. Skip to end of line to set up
      // reading the next entry.
      int c;
      do {
        c = getc(fp);
      } while ((c != EOF) && (c != '\n'));
      if (c == EOF)
        break;
    }

    addr_t region_start, region_end;
    addr_t region_offset;
    char permissions[5] = {'\0'}; // Ensure NUL-terminated string.
    uint8_t dev_major = 0;
    uint8_t dev_minor = 0;
    long inode = 0;
    int path_index = 0;

    // Sample format from man 5 proc:
    //
    // address           perms offset  dev   inode   pathname
    // 08048000-08056000 r-xp 00000000 03:0c 64593   /usr/sbin/gpm
    //
    // The final %n term captures the offset in the input string, which is used
    // to determine the path name. It *does not* increment the return value.
    // Refer to man 3 sscanf for details.
    if (sscanf(line_buffer,
               "%" PRIxPTR "-%" PRIxPTR " %4c "
               "%" PRIxPTR " %hhx:%hhx %ld %n",
               &region_start, &region_end, permissions, &region_offset, &dev_major, &dev_minor, &inode,
               &path_index) < 7) {
      FATAL("/proc/self/maps parse failed!");
      fclose(fp);
      return regions;
    }

    MemoryPermission permission;
    if (permissions[0] == 'r' && permissions[1] == 'w') {
      permission = MemoryPermission::kReadWrite;
    } else if (permissions[0] == 'r' && permissions[2] == 'x') {
      permission = MemoryPermission::kReadExecute;
    } else if (permissions[0] == 'r' && permissions[1] == 'w' && permissions[2] == 'x') {
      permission = MemoryPermission::kReadWriteExecute;
    } else {
      permission = MemoryPermission::kNoAccess;
    }

#if 0
      DLOG(0, "%p --- %p", region_start, region_end);
#endif

    MemRegion region = MemRegion(region_start,region_end - region_start, permission);
    regions.push_back(region);
  }
  std::qsort(&regions[0], regions.size(), sizeof(MemRegion),
             +[](const void* a, const void* b) -> int {
                 const auto *i = static_cast<const MemRegion *>(a);
                 const auto *j = static_cast<const MemRegion *>(b);
                 if ((addr_t)i->start < (addr_t)j->start) return -1;
                 if ((addr_t)i->start > (addr_t)j->start) return 1;
                 return 0;
             });

  fclose(fp);
  return regions;
}

// ================================================================
// GetProcessModuleMap

static std::vector<RuntimeModule> *modules;
static std::vector<RuntimeModule> &get_process_map_with_proc_maps() {
  if(modules == nullptr) {
    modules = new std::vector<RuntimeModule>();
  }

  FILE *fp = fopen("/proc/self/maps", "r");
  if (fp == nullptr)
    return *modules;

  while (!feof(fp)) {
    char line_buffer[LINE_MAX + 1];
    fgets(line_buffer, LINE_MAX, fp);

    // ignore the rest of characters
    if (strlen(line_buffer) == LINE_MAX && line_buffer[LINE_MAX] != '\n') {
      // Entry not describing executable data. Skip to end of line to set up
      // reading the next entry.
      int c;
      do {
        c = getc(fp);
      } while ((c != EOF) && (c != '\n'));
      if (c == EOF)
        break;
    }

    addr_t region_start, region_end;
    addr_t region_offset;
    char permissions[5] = {'\0'}; // Ensure NUL-terminated string.
    uint8_t dev_major = 0;
    uint8_t dev_minor = 0;
    long inode = 0;
    int path_index = 0;

    // Sample format from man 5 proc:
    //
    // address           perms offset  dev   inode   pathname
    // 08048000-08056000 r-xp 00000000 03:0c 64593   /usr/sbin/gpm
    //
    // The final %n term captures the offset in the input string, which is used
    // to determine the path name. It *does not* increment the return value.
    // Refer to man 3 sscanf for details.
    if (sscanf(line_buffer,
               "%" PRIxPTR "-%" PRIxPTR " %4c "
               "%" PRIxPTR " %hhx:%hhx %ld %n",
               &region_start, &region_end, permissions, &region_offset, &dev_major, &dev_minor, &inode,
               &path_index) < 7) {
      FATAL("/proc/self/maps parse failed!");
      fclose(fp);
      return *modules;
    }

    // check header section permission
    if (strcmp(permissions, "r--p") != 0 && strcmp(permissions, "r-xp") != 0)
      continue;

    // check elf magic number
    ElfW(Ehdr) *header = (ElfW(Ehdr) *)region_start;
    if (memcmp(header->e_ident, ELFMAG, SELFMAG) != 0) {
      continue;
    }

    char *path_buffer = line_buffer + path_index;
    if (*path_buffer == 0 || *path_buffer == '\n' || *path_buffer == '[')
      continue;
    RuntimeModule module;

    // strip
    if (path_buffer[strlen(path_buffer) - 1] == '\n') {
      path_buffer[strlen(path_buffer) - 1] = 0;
    }
    strncpy(module.path, path_buffer, sizeof(module.path));
    module.load_address = (void *)region_start;
    modules->push_back(module);

#if 0
    DLOG(0, "module: %s", module.path);
#endif
  }

  fclose(fp);
  return *modules;
}

#if defined(__LP64__)
static std::vector<RuntimeModule> get_process_map_with_linker_iterator() {
  std::vector<RuntimeModule> ProcessModuleMap;

  static int (*dl_iterate_phdr_ptr)(int (*)(struct dl_phdr_info *, size_t, void *), void *);
  dl_iterate_phdr_ptr = (__typeof(dl_iterate_phdr_ptr))dlsym(RTLD_DEFAULT, "dl_iterate_phdr");
  if (dl_iterate_phdr_ptr == NULL) {
    return ProcessModuleMap;
  }

  dl_iterate_phdr_ptr(
      [](dl_phdr_info *info, size_t size, void *data) {
        RuntimeModule module = {0};
        if (info->dlpi_name && info->dlpi_name[0] == '/')
          strcpy(module.path, info->dlpi_name);

        module.load_address = (void *)info->dlpi_addr;
        for (size_t i = 0; i < info->dlpi_phnum; ++i) {
          if (info->dlpi_phdr[i].p_type == PT_LOAD) {
            uintptr_t load_bias = (info->dlpi_phdr[i].p_vaddr - info->dlpi_phdr[i].p_offset);
            module.load_address = (void *)((addr_t)module.load_address + load_bias);
            break;
          }
        }

        // push to vector
        auto ProcessModuleMap = reinterpret_cast<std::vector<RuntimeModule> *>(data);
        ProcessModuleMap->push_back(module);
        return 0;
      },
      (void *)&ProcessModuleMap);

  return ProcessModuleMap;
}
#endif

const std::vector<RuntimeModule> &ProcessRuntimeUtility::GetProcessModuleMap() {
#if defined(__LP64__) && 0
  // TODO: won't resolve main binary
  return get_process_map_with_linker_iterator();
#else
  return get_process_map_with_proc_maps();
#endif
}

RuntimeModule ProcessRuntimeUtility::GetProcessModule(const char *name) {
  auto modules = GetProcessModuleMap();
  for (auto module : modules) {
    if (strstr(module.path, name) != 0) {
      return module;
    }
  }
  return RuntimeModule{0};
}
