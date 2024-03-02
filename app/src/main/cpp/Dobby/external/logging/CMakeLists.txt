include_directories(.)

set(SOURCE_FILE_LIST
  logging.cc
  )

if (DOBBY_BUILD_KERNEL_MODE)
  set(SOURCE_FILE_LIST
    logging_kern.cc
    )
endif ()

get_absolute_path_list(SOURCE_FILE_LIST SOURCE_FILE_LIST_)
set(SOURCE_FILE_LIST ${SOURCE_FILE_LIST_})

add_library(logging
  ${SOURCE_FILE_LIST}
  )