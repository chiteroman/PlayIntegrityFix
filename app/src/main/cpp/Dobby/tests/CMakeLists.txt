
find_package(PkgConfig REQUIRED)
pkg_check_modules(CAPSTONE REQUIRED capstone)
message(STATUS "Capstone libraries: " ${CAPSTONE_LIBRARY_DIRS})
message(STATUS "Capstone includes: " ${CAPSTONE_INCLUDE_DIRS})

pkg_check_modules(UNICORN REQUIRED unicorn)
message(STATUS "unicorn libraries: " ${UNICORN_LIBRARY_DIRS})
message(STATUS "unicorn includes: " ${UNICORN_INCLUDE_DIRS})

get_property(DOBBY_SOURCE_FILE_LIST
  TARGET dobby
  PROPERTY SOURCES)

set(DOBBY_SOURCES)
foreach (path ${DOBBY_SOURCE_FILE_LIST})
  if (NOT path MATCHES "^/")
    list(APPEND DOBBY_SOURCES ${DOBBY_DIR}/${path})
  else ()
    list(APPEND DOBBY_SOURCES ${path})
  endif ()
endforeach ()


add_executable(test_insn_relo_arm64
  test_insn_relo_arm64.cpp
  UniconEmulator.cpp
  ${DOBBY_SOURCES}
  )

target_compile_definitions(test_insn_relo_arm64 PUBLIC
  LOGGING_DEBUG=1
  DISABLE_ARCH_DETECT=1
  TARGET_ARCH_ARM64=1
  TEST_WITH_UNICORN=1
  )

target_include_directories(test_insn_relo_arm64 PUBLIC
  ${CAPSTONE_INCLUDE_DIRS}
  ${UNICORN_INCLUDE_DIRS}
  )

target_link_directories(test_insn_relo_arm64 PUBLIC
  ${CAPSTONE_LIBRARY_DIRS}
  ${UNICORN_LIBRARY_DIRS}
  )

target_link_libraries(test_insn_relo_arm64 PUBLIC
  ${CAPSTONE_LIBRARIES}
  ${UNICORN_LIBRARIES}
  )

# ---

add_executable(test_insn_relo_arm
  test_insn_relo_arm.cpp
  UniconEmulator.cpp
  ${DOBBY_SOURCES}
  )

target_compile_definitions(test_insn_relo_arm PUBLIC
  LOGGING_DEBUG=1
  DISABLE_ARCH_DETECT=1
  TARGET_ARCH_ARM=1
  TEST_WITH_UNICORN=1
  )

target_include_directories(test_insn_relo_arm PUBLIC
  ${CAPSTONE_INCLUDE_DIRS}
  ${UNICORN_INCLUDE_DIRS}
  )

target_link_directories(test_insn_relo_arm PUBLIC
  ${CAPSTONE_LIBRARY_DIRS}
  ${UNICORN_LIBRARY_DIRS}
  )

target_link_libraries(test_insn_relo_arm PUBLIC
  ${CAPSTONE_LIBRARIES}
  ${UNICORN_LIBRARIES}
  )

# ---

add_executable(test_insn_relo_x64
  test_insn_relo_x64.cpp
  UniconEmulator.cpp
  ${DOBBY_SOURCES}
  )

target_compile_definitions(test_insn_relo_x64 PUBLIC
  LOGGING_DEBUG=1
  DISABLE_ARCH_DETECT=1
  TARGET_ARCH_X64=1
  TEST_WITH_UNICORN=1
  # TARGET_ARCH_IA32=1
  )

target_include_directories(test_insn_relo_x64 PUBLIC
  ${CAPSTONE_INCLUDE_DIRS}
  ${UNICORN_INCLUDE_DIRS}
  )

target_link_directories(test_insn_relo_x64 PUBLIC
  ${CAPSTONE_LIBRARY_DIRS}
  ${UNICORN_LIBRARY_DIRS}
  )

target_link_libraries(test_insn_relo_x64 PUBLIC
  ${CAPSTONE_LIBRARIES}
  ${UNICORN_LIBRARIES}
  )

# ---

add_executable(test_native
  test_native.cpp)

target_link_libraries(test_native
  dobby)