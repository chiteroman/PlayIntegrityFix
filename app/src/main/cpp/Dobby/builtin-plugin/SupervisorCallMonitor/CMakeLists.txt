add_library(supervisor_call_monitor STATIC
  mach_system_call_log_handler.cc
  system_call_log_handler.cc
  supervisor_call_monitor.cc
  sensitive_api_monitor.cc
  misc_utility.cc
  )
target_link_libraries(supervisor_call_monitor
  misc_helper
  dobby
  )

add_library(test_supervisor_call_monitor SHARED
  test_supervisor_call_monitor.cc
  )
target_link_libraries(test_supervisor_call_monitor
  supervisor_call_monitor
)

include_directories(
  .
)

