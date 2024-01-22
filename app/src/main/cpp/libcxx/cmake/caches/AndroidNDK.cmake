# Use CMake's built-in Android NDK support. This will also use the compiler from
# the NDK.

set(CMAKE_ANDROID_NDK "$ENV{ANDROID_NDK_HOME}" CACHE STRING "")
set(CMAKE_SYSTEM_NAME Android CACHE STRING "")

# Default architecture and Android API level. Selecting a much earlier API may
# require adding a toybox build to the device's PATH.
set(CMAKE_ANDROID_ARCH_ABI "x86_64" CACHE STRING "")
set(CMAKE_SYSTEM_VERSION 21 CACHE STRING "")

# Rename libc++.so to libc++_shared.so to ensure that it doesn't interfere with
# the libc++.so on the device, then use LD_LIBRARY_PATH to point test
# executables at the library. Also rename the static library for consistency
# and to help prevent -lc++ from linking libc++.a.
set(LIBCXX_SHARED_OUTPUT_NAME c++_shared CACHE STRING "")
set(LIBCXX_STATIC_OUTPUT_NAME c++_static CACHE STRING "")

# Use a different C++ namespace for the NDK libc++_shared.so to help avoid
# symbol conflicts on older versions of Android.
set(LIBCXX_ABI_NAMESPACE __test CACHE STRING "")

# The build doesn't add a suffix to an Android shared object filename, so it
# writes both a libc++_shared.so ELF file and a libc++_shared.so linker script
# to the same output path (the script clobbers the binary). Turn off the linker
# script.
set(LIBCXX_ENABLE_ABI_LINKER_SCRIPT OFF CACHE BOOL "")

set(LIBCXX_STATICALLY_LINK_ABI_IN_SHARED_LIBRARY ON CACHE BOOL "")
set(LIBCXXABI_ENABLE_SHARED OFF CACHE BOOL "")

# Use adb to push tests to a locally-connected device (e.g. emulator) and run them.
set(LIBCXX_TEST_CONFIG "llvm-libc++-android-ndk.cfg.in" CACHE STRING "")
set(LIBCXXABI_TEST_CONFIG "llvm-libc++abi-android-ndk.cfg.in" CACHE STRING "")

# CMAKE_SOURCE_DIR refers to the "<monorepo>/runtimes" directory.
set(LIBCXX_EXECUTOR "${CMAKE_SOURCE_DIR}/../libcxx/utils/adb_run.py" CACHE STRING "")
set(LIBCXXABI_EXECUTOR "${LIBCXX_EXECUTOR}" CACHE STRING "")
