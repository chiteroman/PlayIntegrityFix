
#include "dobby/dobby_internal.h"

#include "SupervisorCallMonitor/supervisor_call_monitor.h"

#if 1
__attribute__((constructor)) static void ctor() {
  log_set_level(2);
  log_switch_to_syslog();

  supervisor_call_monitor_init();
  supervisor_call_monitor_register_main_app();
  supervisor_call_monitor_register_syscall_call_log_handler();
  supervisor_call_monitor_register_mach_syscall_call_log_handler();
}
#endif