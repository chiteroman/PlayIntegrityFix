set(SOURCE_FILE_LIST)

if (NOT DEFINED DOBBY_DIR)
  message(FATAL_ERROR "DOBBY_DIR must be set!")
endif ()

if (SYSTEM.Darwin AND (NOT DOBBY_BUILD_KERNEL_MODE))
  set(SOURCE_FILE_LIST ${SOURCE_FILE_LIST}
    ${CMAKE_CURRENT_SOURCE_DIR}/macho/macho_ctx.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/macho/shared_cache_ctx.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/macho/dobby_symbol_resolver.cc


    ${DOBBY_DIR}/source/Backend/UserMode/PlatformUtil/Darwin/ProcessRuntimeUtility.cc
    )
endif ()
if (SYSTEM.Darwin AND DOBBY_BUILD_KERNEL_MODE)
  set(SOURCE_FILE_LIST ${SOURCE_FILE_LIST}
    ${CMAKE_CURRENT_SOURCE_DIR}/macho/dobby_symbol_resolver.cc

    ${DOBBY_DIR}/source/Backend/KernelMode/PlatformUtil/Darwin/ProcessRuntimeUtility.cc
    )
endif ()
if (SYSTEM.Linux OR SYSTEM.Android)
  set(SOURCE_FILE_LIST ${SOURCE_FILE_LIST}
    ${CMAKE_CURRENT_SOURCE_DIR}/elf/dobby_symbol_resolver.cc

    ${DOBBY_DIR}/source/Backend/UserMode/PlatformUtil/Linux/ProcessRuntimeUtility.cc
    )
endif ()
if (SYSTEM.Windows)
  set(SOURCE_FILE_LIST ${SOURCE_FILE_LIST}
    ${CMAKE_CURRENT_SOURCE_DIR}/pe/dobby_symbol_resolver.cc

    ${DOBBY_DIR}/source/Backend/UserMode/PlatformUtil/Windows/ProcessRuntimeUtility.cc
    )
endif ()

add_library(macho_ctx_kit
  macho/macho_ctx.h
  macho/macho_ctx.cc
  )

add_library(shared_cache_ctx_kit
  macho/shared_cache_ctx.h
  macho/shared_cache_ctx.cpp
  )

add_library(dobby_symbol_resolver
  ${SOURCE_FILE_LIST}
  )


include_directories(
  .
)

