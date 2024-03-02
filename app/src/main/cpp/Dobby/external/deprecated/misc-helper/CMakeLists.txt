include_directories(.)

if(NOT DOBBY_BUILD_KERNEL_MODE)
  set(SOURCE_FILE_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/variable_cache.c
    ${CMAKE_CURRENT_SOURCE_DIR}/async_logger.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/format_printer.cc
    )
else()
  set(SOURCE_FILE_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/format_printer.cc
    )
endif()

add_library(misc_helper
  ${SOURCE_FILE_LIST}
  ${SOURCE_HEADER_LIST}
)