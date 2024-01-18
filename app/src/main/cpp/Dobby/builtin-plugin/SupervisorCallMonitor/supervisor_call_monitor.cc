#include "SupervisorCallMonitor/misc_utility.h"
#include "dobby_internal.h"
#include "PlatformUtil/ProcessRuntimeUtility.h"

#include "misc-helper/async_logger.h"

#include <vector>
std::vector<DBICallTy> *g_supervisor_call_handlers;

static const char *fast_get_main_app_bundle_udid() {
  static char *main_app_bundle_udid = NULL;
  if (main_app_bundle_udid)
    return main_app_bundle_udid;

  auto main = ProcessRuntimeUtility::GetProcessModuleMap()[0];
  char main_binary_path[2048] = {0};
  if (realpath(main.path, main_binary_path) == NULL)
    return NULL;

  char *bundle_udid_ndx = main_binary_path + strlen("/private/var/containers/Bundle/Application/");
  main_app_bundle_udid = (char *)malloc(36 + 1);
  strncpy(main_app_bundle_udid, bundle_udid_ndx, 36);
  main_app_bundle_udid[36] = 0;
  return main_app_bundle_udid;
}

static void common_supervisor_call_monitor_handler(DobbyRegisterContext *ctx, const InterceptEntry *info) {
  if (g_supervisor_call_handlers == NULL) {
    return;
  }
  for (auto handler : *g_supervisor_call_handlers) {
    handler(ctx, info);
  }
}

void supervisor_call_monitor_register_handler(DBICallTy handler) {
  if (g_supervisor_call_handlers == NULL) {
    g_supervisor_call_handlers = new std::vector<DBICallTy>();
  }
  g_supervisor_call_handlers->push_back(handler);
}

std::vector<addr_t> *g_svc_addr_array;

void supervisor_call_monitor_register_svc(addr_t svc_addr) {
  if (g_svc_addr_array == NULL) {
    g_svc_addr_array = new std::vector<addr_t>();
  }

  if (g_svc_addr_array) {
    auto iter = g_svc_addr_array->begin();
    for (; iter != g_svc_addr_array->end(); iter++) {
      if (*iter == svc_addr)
        return;
    }
  }

  g_svc_addr_array->push_back(svc_addr);
  DobbyInstrument((void *)svc_addr, common_supervisor_call_monitor_handler);
  DLOG(2, "register supervisor_call_monitor at %p", svc_addr);
}

void supervisor_call_monitor_register_image(void *header) {
  auto text_section = macho_kit_get_section_by_name((mach_header_t *)header, "__TEXT", "__text");

  addr_t insn_addr = (addr_t)header + (addr_t)text_section->offset;
  addr_t insn_addr_end = insn_addr + text_section->size;

  for (; insn_addr < insn_addr_end; insn_addr += sizeof(uint32_t)) {
    if (*(uint32_t *)insn_addr == 0xd4001001) {
      supervisor_call_monitor_register_svc((addr_t)insn_addr);
    }
  }
}

void supervisor_call_monitor_register_main_app() {
  const char *main_bundle_udid = fast_get_main_app_bundle_udid();
  auto module_map = ProcessRuntimeUtility::GetProcessModuleMap();
  for (auto module : module_map) {
    if (strstr(module.path, main_bundle_udid)) {
      LOG(2, "[supervisor_call_monitor] %s", module.path);
      supervisor_call_monitor_register_image((void *)module.load_address);
    }
  }
}

extern "C" int __shared_region_check_np(uint64_t *startaddress);

struct dyld_cache_header *shared_cache_get_load_addr() {
  static struct dyld_cache_header *shared_cache_load_addr = 0;
  if (shared_cache_load_addr)
    return shared_cache_load_addr;
#if 0
  if (syscall(294, &shared_cache_load_addr) == 0) {
#else
  // FIXME:
  if (__shared_region_check_np((uint64_t *)&shared_cache_load_addr) != 0) {
#endif
  shared_cache_load_addr = 0;
}
return shared_cache_load_addr;
}
void supervisor_call_monitor_register_system_kernel() {
  auto libsystem = ProcessRuntimeUtility::GetProcessModule("libsystem_kernel.dylib");
  addr_t libsystem_header = (addr_t)libsystem.load_address;
  auto text_section = macho_kit_get_section_by_name((mach_header_t *)libsystem_header, "__TEXT", "__text");

  addr_t shared_cache_load_addr = (addr_t)shared_cache_get_load_addr();
  addr_t insn_addr = shared_cache_load_addr + (addr_t)text_section->offset;
  addr_t insn_addr_end = insn_addr + text_section->size;

  addr_t write_svc_addr = (addr_t)DobbySymbolResolver("libsystem_kernel.dylib", "write");
  write_svc_addr += 4;

  addr_t __psynch_mutexwait_svc_addr = (addr_t)DobbySymbolResolver("libsystem_kernel.dylib", "__psynch_mutexwait");
  __psynch_mutexwait_svc_addr += 4;

  for (; insn_addr < insn_addr_end; insn_addr += sizeof(uint32_t)) {
    if (*(uint32_t *)insn_addr == 0xd4001001) {
      if (insn_addr == write_svc_addr)
        continue;

      if (insn_addr == __psynch_mutexwait_svc_addr)
        continue;
      supervisor_call_monitor_register_svc((addr_t)insn_addr);
    }
  }
}

void supervisor_call_monitor_init() {
  // create logger file
  char logger_path[1024] = {0};
  sprintf(logger_path, "%s%s", getenv("HOME"), "/Documents/svc_monitor.txt");
  LOG(2, "HOME: %s", logger_path);
  async_logger_init(logger_path);

  dobby_enable_near_branch_trampoline();
}
