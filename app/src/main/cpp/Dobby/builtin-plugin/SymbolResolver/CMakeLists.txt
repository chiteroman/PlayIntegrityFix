set(SOURCE_FILE_LIST)

include_directories(
  .
)

if (NOT DEFINED DOBBY_DIR)
  message(FATAL_ERROR "DOBBY_DIR must be set!")
endif ()

if (SYSTEM.Darwin)
  add_library(macho_ctx_kit
    ${CMAKE_CURRENT_SOURCE_DIR}/macho/macho_ctx.h
    ${CMAKE_CURRENT_SOURCE_DIR}/macho/macho_ctx.cc
    )

  add_library(shared_cache_ctx_kit
    ${CMAKE_CURRENT_SOURCE_DIR}/macho/shared_cache_ctx.h
    ${CMAKE_CURRENT_SOURCE_DIR}/macho/shared_cache_ctx.cpp
    )

  set(SOURCE_FILE_LIST ${SOURCE_FILE_LIST}
    macho/macho_ctx.cc
    macho/dobby_symbol_resolver.cc
    )

  if (NOT DOBBY_BUILD_KERNEL_MODE)
    set(SOURCE_FILE_LIST ${SOURCE_FILE_LIST}
      macho/macho_file_symbol_resolver.cpp
      macho/shared_cache_ctx.cpp
      ${DOBBY_DIR}/source/Backend/UserMode/PlatformUtil/Darwin/ProcessRuntimeUtility.cc
      )
  endif ()
elseif (SYSTEM.Linux OR SYSTEM.Android)
  set(SOURCE_FILE_LIST ${SOURCE_FILE_LIST}
    elf/dobby_symbol_resolver.cc

    ${DOBBY_DIR}/source/Backend/UserMode/PlatformUtil/Linux/ProcessRuntimeUtility.cc
    )
elseif (SYSTEM.Windows)
  set(SOURCE_FILE_LIST ${SOURCE_FILE_LIST}
    pe/dobby_symbol_resolver.cc

    ${DOBBY_DIR}/source/Backend/UserMode/PlatformUtil/Windows/ProcessRuntimeUtility.cc
    )
endif ()

get_absolute_path_list(SOURCE_FILE_LIST SOURCE_FILE_LIST_)
set(SOURCE_FILE_LIST ${SOURCE_FILE_LIST_})

add_library(dobby_symbol_resolver
  ${SOURCE_FILE_LIST}
  )


