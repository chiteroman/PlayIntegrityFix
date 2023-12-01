# set(CMAKE_BUILD_WITH_INSTALL_NAME_DIR TRUE)
set(CMAKE_INSTALL_NAME_DIR "@rpath")
set(CMAKE_SHARED_LIBRARY_RUNTIME_C_FLAG "-Wl,-rpath,")
add_library(DobbyX ${DOBBY_LIBRARY_TYPE} ${dobby.HEADER_FILE_LIST} ${dobby.SOURCE_FILE_LIST} ${logging.SOURCE_FILE_LIST} ${misc_helper.SOURCE_FILE_LIST} ${dobby.plugin.SOURCE_FILE_LIST})

set_target_properties(DobbyX
  PROPERTIES
  LINK_FLAGS "${linker_flags}"
  COMPILE_FLAGS "${compiler_flags}"
  )

# set framework property
set_target_properties(DobbyX PROPERTIES
  FRAMEWORK TRUE
  FRAMEWORK_VERSION A
  MACOSX_FRAMEWORK_IDENTIFIER "com.dobby.dobby"
  # MACOSX_FRAMEWORK_INFO_PLIST Info.plist
  VERSION 1.0.0 # current version
  SOVERSION 1.0.0 # compatibility version
  PUBLIC_HEADER include/dobby.h
  XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development"
  )

if ((SYSTEM.Darwin AND BUILDING_PLUGIN) AND (NOT DOBBY_BUILD_KERNEL_MODE))
add_subdirectory(builtin-plugin/Dyld2HideLibrary)
add_subdirectory(builtin-plugin/ObjcRuntimeHook)
if (PROCESSOR.AARCH64)
  add_subdirectory(builtin-plugin/SupervisorCallMonitor)
endif ()
endif()