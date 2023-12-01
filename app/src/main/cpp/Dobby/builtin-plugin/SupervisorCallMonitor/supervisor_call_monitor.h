#pragma once

#include <stdint.h>
typedef uintptr_t addr_t;

#include "dobby.h"

void supervisor_call_monitor_init();

void supervisor_call_monitor_register_handler(DBICallTy handler);

void supervisor_call_monitor_register_svc(addr_t svc_addr);

void supervisor_call_monitor_register_image(void *header);

void supervisor_call_monitor_register_main_app();

void supervisor_call_monitor_register_system_kernel();

void supervisor_call_monitor_register_syscall_call_log_handler();

void supervisor_call_monitor_register_mach_syscall_call_log_handler();

void supervisor_call_monitor_register_sensitive_api_handler();