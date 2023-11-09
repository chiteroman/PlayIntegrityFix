#include "dobby/dobby_internal.h"

#include <sys/param.h>
#include <mach/mach.h>
#include <sys/syscall.h>

#include "SupervisorCallMonitor/supervisor_call_monitor.h"
#include "misc-helper/async_logger.h"

#define PT_DENY_ATTACH 31

static void sensitive_api_handler(DobbyRegisterContext *ctx, const InterceptEntry *info) {
  char buffer[256] = {0};
  int syscall_rum = ctx->general.regs.x16;
  if (syscall_rum == 0) {
    syscall_rum = (int)ctx->general.x[0];
    if (syscall_rum == SYS_ptrace) {
      int request = ctx->general.x[1];
      if (request == PT_DENY_ATTACH) {
        ctx->general.x[1] = 0;
        // INFO_LOG("syscall svc ptrace deny");
      }
    }
    if (syscall_rum == SYS_exit) {
      // INFO_LOG("syscall svc exit");
    }
  } else if (syscall_rum > 0) {
    if (syscall_rum == SYS_ptrace) {
      int request = ctx->general.x[0];
      if (request == PT_DENY_ATTACH) {
        ctx->general.x[0] = 0;
        // INFO_LOG("svc ptrace deny");
      }
    }
    if (syscall_rum == SYS_exit) {
      // INFO_LOG("svc exit");
    }
  }
  async_logger_print(buffer);
}

static int get_func_svc_offset(addr_t func_addr) {
  typedef int32_t arm64_instr_t;
  for (int i = 0; i < 8; i++) {
    arm64_instr_t *insn = (arm64_instr_t *)func_addr + i;
    if (*insn == 0xd4001001) {
      return i * sizeof(arm64_instr_t);
    }
  }
  return 0;
}

#include <sys/sysctl.h>
__typeof(sysctl) *orig_sysctl;
int fake_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp, size_t newlen) {
  struct kinfo_proc *info = NULL;
  int ret = orig_sysctl(name, namelen, oldp, oldlenp, newp, newlen);
  if (name[0] == CTL_KERN && name[1] == KERN_PROC && name[2] == KERN_PROC_PID) {
    info = (struct kinfo_proc *)oldp;
    info->kp_proc.p_flag &= ~(P_TRACED);
  }
  return ret;
}

void supervisor_call_monitor_register_sensitive_api_handler() {
  char *sensitive_func_array[] = {"ptrace", "exit"};
  size_t count = sizeof(sensitive_func_array) / sizeof(char *);
  for (size_t i = 0; i < count; i++) {

    addr_t func_addr = 0;

    char func_name[64] = {0};
    sprintf(func_name, "__%s", sensitive_func_array[i]);
    func_addr = (addr_t)DobbySymbolResolver("libsystem_kernel.dylib", func_name);
    if (func_addr == 0) {
      func_addr = (addr_t)DobbySymbolResolver("libsystem_kernel.dylib", sensitive_func_array[i]);
    }
    if (func_addr == 0) {
      INFO_LOG("not found func %s", sensitive_func_array[i]);
      continue;
    }
    int func_svc_offset = get_func_svc_offset(func_addr);
    if (func_svc_offset == 0) {
      INFO_LOG("not found svc %s", sensitive_func_array[i]);
      continue;
    }
    addr_t func_svc_addr = func_addr + func_svc_offset;
    supervisor_call_monitor_register_svc(func_svc_addr);
  }

  // ===============
  DobbyHook((void *)sysctl, (void *)fake_sysctl, (void **)&orig_sysctl);

  supervisor_call_monitor_register_handler(sensitive_api_handler);
}
