#include "dobby_internal.h"

#include <mach/mach.h>

#include "misc-helper/async_logger.h"
#include "PlatformUtil/ProcessRuntimeUtility.h"
#include "SupervisorCallMonitor/misc_utility.h"
#include "SupervisorCallMonitor/supervisor_call_monitor.h"

#include "XnuInternal/syscalls.c"

static const char *syscall_num_to_str(int num) {
  return syscallnames[num];
}

static addr_t getCallFirstArg(DobbyRegisterContext *ctx) {
  addr_t result;
#if defined(_M_X64) || defined(__x86_64__)
#if defined(_WIN32)
  result = ctx->general.regs.rcx;
#else
  result = ctx->general.regs.rdi;
#endif
#elif defined(__arm64__) || defined(__aarch64__)
  result = ctx->general.regs.x0;
#elif defined(__arm__)
  result = ctx->general.regs.r0;
#else
#error "Not Support Architecture."
#endif
  return result;
}

static addr_t getRealLr(DobbyRegisterContext *ctx) {
  addr_t closure_trampoline_reserved_stack = ctx->sp - sizeof(addr_t);
  return *(addr_t *)closure_trampoline_reserved_stack;
}

static addr_t fast_get_caller_from_main_binary(DobbyRegisterContext *ctx) {
  static addr_t text_section_start = 0, text_section_end = 0;
  static addr_t slide = 0;
  if (text_section_start == 0 || text_section_end == 0) {
    auto main = ProcessRuntimeUtility::GetProcessModule("");
    addr_t main_header = (addr_t)main.load_address;

    auto text_segment = macho_kit_get_segment_by_name((mach_header_t *)main_header, "__TEXT");
    slide = main_header - text_segment->vmaddr;

    auto text_section = macho_kit_get_section_by_name((mach_header_t *)main_header, "__TEXT", "__text");
    text_section_start = main_header + (addr_t)text_section->offset;
    text_section_end = text_section_start + text_section->size;
  }

  if (ctx == NULL)
    return 0;

  addr_t lr = getRealLr(ctx);
  if (lr > text_section_start && lr < text_section_end)
    return lr - slide;

#define MAX_STACK_ITERATE_LEVEL 8
  addr_t fp = ctx->fp;
  if (fp == 0)
    return 0;
  for (int i = 0; i < MAX_STACK_ITERATE_LEVEL; i++) {
    addr_t lr = *(addr_t *)(fp + sizeof(addr_t));
    if (lr > text_section_start && lr < text_section_end)
      return lr - slide;
    fp = *(addr_t *)fp;
    if (fp == 0)
      return 0;
  }
  return 0;
}

static void syscall_log_handler(DobbyRegisterContext *ctx, const InterceptEntry *info) {
  addr_t caller = fast_get_caller_from_main_binary(ctx);
  if (caller == 0)
    return;

  char buffer[2048] = {0};
  int syscall_rum = ctx->general.regs.x16;
  if (syscall_rum == 0) {
    syscall_rum = (int)getCallFirstArg(ctx);
    sprintf(buffer, "[syscall svc-%d] %s\n", syscall_rum, syscall_num_to_str(syscall_rum));
  } else if (syscall_rum > 0) {
    sprintf(buffer, "[svc-%d] %s\n", syscall_rum, syscall_num_to_str(syscall_rum));
    if (syscall_rum == 5) {
      sprintf(buffer, "[svc-%d] %s:%s\n", syscall_rum, syscall_num_to_str(syscall_rum), (char *)ctx->general.regs.x0);
    }
  }
  async_logger_print(buffer);
}

void supervisor_call_monitor_register_syscall_call_log_handler() {
  fast_get_caller_from_main_binary(NULL);
  supervisor_call_monitor_register_handler(syscall_log_handler);
}