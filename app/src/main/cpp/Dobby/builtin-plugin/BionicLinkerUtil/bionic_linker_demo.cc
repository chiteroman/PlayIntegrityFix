#include "dobby.h"

#include "bionic_linker_util.h"

#include "logging/logging.h"

#include <dlfcn.h>

#define LOG_TAG "BionicLinkerUtil"

__attribute__((constructor)) static void ctor() {
  const char *lib = NULL;

#if defined(__LP64__)
  lib = "/system/lib64/libandroid_runtime.so";
#else
  lib = "/system/lib/libandroid_runtime.so";
#endif

  void *vm = NULL;

  vm = DobbySymbolResolver(lib, "_ZN7android14AndroidRuntime7mJavaVME");
  INFO_LOG("DobbySymbolResolver::vm %p", vm);

#if 0
  linker_disable_namespace_restriction();
  void *handle = NULL;
  handle       = dlopen(lib, RTLD_LAZY);
  vm           = dlsym(handle, "_ZN7android14AndroidRuntime7mJavaVME");
#else
  void *handle = NULL;
  handle = linker_dlopen(lib, RTLD_LAZY);
  vm = dlsym(handle, "_ZN7android14AndroidRuntime7mJavaVME");
#endif
  INFO_LOG("vm %p", vm);
}
