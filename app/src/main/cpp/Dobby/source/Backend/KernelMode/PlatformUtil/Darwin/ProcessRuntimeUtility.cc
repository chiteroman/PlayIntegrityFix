#include "PlatformUtil/ProcessRuntimeUtility.h"

#include <mach/mach_types.h>

typedef struct _loaded_kext_summary {
  char        name[KMOD_MAX_NAME];
  uuid_t      uuid;
  uint64_t    address;
  uint64_t    size;
  uint64_t    version;
  uint32_t    loadTag;
  uint32_t    flags;
  uint64_t    reference_list;
  uint64_t    text_exec_address;
  size_t      text_exec_size;
} OSKextLoadedKextSummary;
typedef struct _loaded_kext_summary_header {
  uint32_t version;
  uint32_t entry_size;
  uint32_t numSummaries;
  uint32_t reserved; /* explicit alignment for gdb  */
  OSKextLoadedKextSummary summaries[0];
} OSKextLoadedKextSummaryHeader;

#undef min
#undef max
#include <IOKit/IOLib.h>
#include <mach/mach_vm.h>

#include <mach-o/loader.h>
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

// Generate the name for an offset.
#define KERN_PARAM_OFFSET(type_, member_) __##type_##__##member_##__offset_
#define KERN_STRUCT_OFFSET KERN_PARAM_OFFSET

struct vm_map_links {
  struct vm_map_entry *prev;
  struct vm_map_entry *next;
  vm_map_offset_t start;
  vm_map_offset_t end;
};

struct vm_map_header {
  struct vm_map_links links;
  uint8_t placeholder_[];
};

static inline vm_map_offset_t vme_start(vm_map_entry_t entry) {
  uint KERN_STRUCT_OFFSET(vm_map_entry, links) = 0;
  return ((vm_map_header *)((addr_t)entry + KERN_STRUCT_OFFSET(vm_map_entry, links)))->links.start;
}
static inline vm_map_entry_t vm_map_to_entry(vm_map_t map) {
  return nullptr;
}
static inline vm_map_entry_t vm_map_first_entry(vm_map_t map) {
  uint KERN_STRUCT_OFFSET(vm_map, hdr) = 4;
  return ((vm_map_header *)((addr_t)map + KERN_STRUCT_OFFSET(vm_map, hdr)))->links.next;
}

// ---

static std::vector<MemRegion> regions;
const std::vector<MemRegion> &ProcessRuntimeUtility::GetProcessMemoryLayout() {
  return regions;
}

// ---

#include <libkern/OSKextLib.h>

extern "C" void *kernel_info_load_base();;

std::vector<RuntimeModule> modules;
const std::vector<RuntimeModule> *ProcessRuntimeUtility::GetProcessModuleMap() {
  modules.clear();

  // brute force kernel base ? so rude :)
  static void *kernel_base = nullptr;
  static OSKextLoadedKextSummaryHeader *_gLoadedKextSummaries = nullptr;
  if (kernel_base == nullptr) {
    kernel_base = kernel_info_load_base();
    if (kernel_base == nullptr) {
      ERROR_LOG("kernel base not found");
      return &modules;
    }
    LOG(0, "kernel base at: %p", kernel_base);

    extern void *DobbyMachOSymbolResolver(void *header_, const char *symbol_name);
    OSKextLoadedKextSummaryHeader **_gLoadedKextSummariesPtr;
    _gLoadedKextSummariesPtr = (typeof(_gLoadedKextSummariesPtr))DobbyMachOSymbolResolver(kernel_base, "_gLoadedKextSummaries");
    if (_gLoadedKextSummariesPtr == nullptr) {
      ERROR_LOG("failed resolve gLoadedKextSummaries symbol");
      return &modules;
    }
    _gLoadedKextSummaries = *_gLoadedKextSummariesPtr;
    LOG(0, "gLoadedKextSummaries at: %p", _gLoadedKextSummaries);
  }

  // only kernel
  RuntimeModule module = {0};
  strncpy(module.path, "kernel", sizeof(module.path));
  module.load_address = (void *)kernel_base;
  modules.push_back(module);

  // kext
  for (int i = 0; i < _gLoadedKextSummaries->numSummaries; ++i) {
    strncpy(module.path, _gLoadedKextSummaries->summaries[i].name, sizeof(module.path));
    module.load_address = (void *)_gLoadedKextSummaries->summaries[i].address;
    modules.push_back(module);
  }

  return &modules;
}

RuntimeModule ProcessRuntimeUtility::GetProcessModule(const char *name) {
  const std::vector<RuntimeModule> *modules = GetProcessModuleMap();
  return RuntimeModule{0};
}
