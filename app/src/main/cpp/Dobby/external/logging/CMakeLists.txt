include_directories(.)

if(NOT DOBBY_BUILD_KERNEL_MODE)
  set(SOURCE_FILE_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/logging.cc
  )
else()
  set(SOURCE_FILE_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_logging.cc
    )
endif()
add_library(logging
  ${SOURCE_FILE_LIST}
  ${SOURCE_HEADER_LIST}
)