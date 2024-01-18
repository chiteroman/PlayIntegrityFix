#include <stdlib.h> /* getenv */
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <fstream>

#include <set>
#include <unordered_map>

#include <dlfcn.h>
#include <sys/param.h>

#include "dobby.h"

#include "common_header.h"

#define LOG_TAG "DynamicLoaderMonitor"

std::unordered_map<void *, const char *> traced_dlopen_handle_list;

static void *(*orig_dlopen)(const char *__file, int __mode);
static void *fake_dlopen(const char *__file, int __mode) {
  void *result = orig_dlopen(__file, __mode);
  if (result != NULL && __file) {
    char *traced_filename = (char *)malloc(MAXPATHLEN);
    // FIXME: strncpy
    strcpy(traced_filename, __file);
    LOG(1, "[-] dlopen handle: %s", __file);
    traced_dlopen_handle_list.insert(std::make_pair(result, (const char *)traced_filename));
  }
  return result;
}

static void *(*orig_loader_dlopen)(const char *filename, int flags, const void *caller_addr);
static void *fake_loader_dlopen(const char *filename, int flags, const void *caller_addr) {
  void *result = orig_loader_dlopen(filename, flags, caller_addr);
  if (result != NULL) {
    char *traced_filename = (char *)malloc(MAXPATHLEN);
    // FIXME: strncpy
    strcpy(traced_filename, filename);
    LOG(1, "[-] dlopen handle: %s", filename);
    traced_dlopen_handle_list.insert(std::make_pair(result, (const char *)traced_filename));
  }
  return result;
}

static const char *get_traced_filename(void *handle, bool removed) {
  std::unordered_map<void *, const char *>::iterator it;
  it = traced_dlopen_handle_list.find(handle);
  if (it != traced_dlopen_handle_list.end()) {
    if (removed)
      traced_dlopen_handle_list.erase(it);
    return it->second;
  }
  return NULL;
}

static void *(*orig_dlsym)(void *__handle, const char *__symbol);
static void *fake_dlsym(void *__handle, const char *__symbol) {
  const char *traced_filename = get_traced_filename(__handle, false);
  if (traced_filename) {
    LOG(1, "[-] dlsym: %s, symbol: %s", traced_filename, __symbol);
  }
  return orig_dlsym(__handle, __symbol);
}

static int (*orig_dlclose)(void *__handle);
static int fake_dlclose(void *__handle) {
  const char *traced_filename = get_traced_filename(__handle, true);
  if (traced_filename) {
    LOG(1, "[-] dlclose: %s", traced_filename);
    free((void *)traced_filename);
  }
  return orig_dlclose(__handle);
}

#if 0
__attribute__((constructor)) static void ctor() {
#if defined(__ANDROID__)
#if 0
  void *dl              = dlopen("libdl.so", RTLD_LAZY);
  void *__loader_dlopen = dlsym(dl, "__loader_dlopen");
#endif
  DobbyHook((void *)DobbySymbolResolver(NULL, "__loader_dlopen"), (void *)fake_loader_dlopen,
            (void **)&orig_loader_dlopen);
#else
  DobbyHook((void *)DobbySymbolResolver(NULL, "dlopen"), (void *)fake_dlopen, (void **)&orig_dlopen);
#endif

  DobbyHook((void *)dlsym, (void *)fake_dlsym, (void **)&orig_dlsym);
  DobbyHook((void *)dlclose, (void *)fake_dlclose, (void **)&orig_dlclose);
}
#endif
