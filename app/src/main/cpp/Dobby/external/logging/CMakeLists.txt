include_directories(.)

if(NOT BUILDING_KERNEL)
  set(SOURCE_FILE_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/cxxlogging.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/logging.c
  )
else()
  set(SOURCE_FILE_LIST
    ${CMAKE_CURRENT_SOURCE_DIR}/cxxlogging.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_logging.c
    )
endif()
add_library(logging
  ${SOURCE_FILE_LIST}
  ${SOURCE_HEADER_LIST}
)