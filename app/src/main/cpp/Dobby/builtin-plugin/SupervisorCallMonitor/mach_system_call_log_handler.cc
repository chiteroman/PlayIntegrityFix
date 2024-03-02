#include "dobby/dobby_internal.h"

#include <iostream>

#include <unistd.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mach/mach.h>

#include "misc-helper/async_logger.h"
#include "PlatformUtil/ProcessRuntimeUtility.h"
#include "SupervisorCallMonitor/misc_utility.h"
#include "SupervisorCallMonitor/supervisor_call_monitor.h"

#include "XnuInternal/syscall_sw.c"

#include "XnuInternal/mach/clock_priv.h"
#include "XnuInternal/mach/clock_reply.h"
#include "XnuInternal/mach/clock.h"
#include "XnuInternal/mach/exc.h"
#include "XnuInternal/mach/host_priv.h"
#include "XnuInternal/mach/host_security.h"
#include "XnuInternal/mach/lock_set.h"
#include "XnuInternal/mach/mach_host.h"
#include "XnuInternal/mach/mach_port.h"
#include "XnuInternal/mach/mach_vm.h"
#include "XnuInternal/mach/mach_voucher.h"
#include "XnuInternal/mach/memory_entry.h"
#include "XnuInternal/mach/processor_set.h"
#include "XnuInternal/mach/processor.h"
#include "XnuInternal/mach/task.h"
#include "XnuInternal/mach/thread_act.h"
#include "XnuInternal/mach/vm_map.h"

typedef struct {
  char *mach_msg_name;
  int mach_msg_id;
} mach_msg_entry_t;

// clang-format off
mach_msg_entry_t mach_msg_array[] = {
    subsystem_to_name_map_clock_priv,
    subsystem_to_name_map_clock_reply,
    subsystem_to_name_map_clock,
    subsystem_to_name_map_exc,
    subsystem_to_name_map_host_priv,
    subsystem_to_name_map_host_security,
    subsystem_to_name_map_lock_set,
    subsystem_to_name_map_mach_host,
    subsystem_to_name_map_mach_port,
    subsystem_to_name_map_mach_vm,
    subsystem_to_name_map_mach_voucher,
    subsystem_to_name_map_memory_entry,
    subsystem_to_name_map_processor_set,
    subsystem_to_name_map_processor,
    subsystem_to_name_map_task,
    subsystem_to_name_map_thread_act,
    subsystem_to_name_map_vm_map,
};
// clang-format on

#define PRIME_NUMBER 8387
char *mach_msg_name_table[PRIME_NUMBER] = {0};
static int hash_mach_msg_num_to_ndx(int mach_msg_num) {
  return mach_msg_num % PRIME_NUMBER;
}
static void mach_msg_id_hash_table_init() {
  static bool initialized = false;
  if (initialized == true) {
    return;
  }
  initialized = true;

  int count = sizeof(mach_msg_array) / sizeof(mach_msg_array[0]);
  for (size_t i = 0; i < count; i++) {
    mach_msg_entry_t entry = mach_msg_array[i];
    int ndx = hash_mach_msg_num_to_ndx(entry.mach_msg_id);
    mach_msg_name_table[ndx] = entry.mach_msg_name;
  }
}

const char *mach_syscall_num_to_str(int num) {
  return mach_syscall_name_table[0 - num];
}

char *mach_msg_id_to_str(int msgh_id) {
  int ndx = hash_mach_msg_num_to_ndx(msgh_id);
  return mach_msg_name_table[ndx];
}

char *mach_msg_to_str(mach_msg_header_t *msg) {
  static mach_port_t self_port = MACH_PORT_NULL;

  if (self_port == MACH_PORT_NULL) {
    self_port = mach_task_self();
  }

  if (msg->msgh_remote_port == self_port) {
    return mach_msg_id_to_str(msg->msgh_id);
  }
  return NULL;
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
    auto main = ProcessRuntimeUtility::GetProcessModule("mobilex");
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

static void mach_syscall_log_handler(DobbyRegisterContext *ctx, const InterceptEntry *info) {
  addr_t caller = fast_get_caller_from_main_binary(ctx);
  if (caller == 0)
    return;

  char buffer[256] = {0};
  int syscall_rum = ctx->general.regs.x16;
  if (syscall_rum == -31) {
    // mach_msg_trap
    mach_msg_header_t *msg = (typeof(msg))getCallFirstArg(ctx);
    char *mach_msg_name = mach_msg_to_str(msg);
    if (mach_msg_name) {
      sprintf(buffer, "[mach msg svc] %s\n", mach_msg_name);
    } else {
      buffer[0] = 0;
    }
  } else if (syscall_rum < 0) {
    sprintf(buffer, "[mach svc-%d] %s\n", syscall_rum, mach_syscall_num_to_str(syscall_rum));
  }
  async_logger_print(buffer);
}

void supervisor_call_monitor_register_mach_syscall_call_log_handler() {
  mach_msg_id_hash_table_init();
  fast_get_caller_from_main_binary(NULL);
  supervisor_call_monitor_register_handler(mach_syscall_log_handler);
}
