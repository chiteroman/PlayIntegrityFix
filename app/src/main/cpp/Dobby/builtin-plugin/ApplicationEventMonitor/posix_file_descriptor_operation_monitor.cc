#include <stdlib.h> /* getenv */
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>

#include <dlfcn.h>

#include <iostream>
#include <fstream>

#include <set>
#include <unordered_map>

#include <sys/param.h>

#include "dobby.h"
#include "common_header.h"

#define LOG_TAG "PosixFileOperationMonitor"

std::unordered_map<int, const char *> *posix_file_descriptors;

int (*orig_open)(const char *pathname, int flags, ...);
int fake_open(const char *pathname, int flags, ...) {
  mode_t mode = 0;
  if (flags & O_CREAT) {
    va_list args;
    va_start(args, flags);
    mode = (mode_t)va_arg(args, int);
    va_end(args);
  }

  int result = orig_open(pathname, flags, mode);
  if (result != -1) {
    char *traced_filename = (char *)malloc(MAXPATHLEN);
    // FIXME: strncpy
    strcpy(traced_filename, pathname);
    LOG(1, "[-] trace open handle: %s", pathname);

    if (posix_file_descriptors == NULL) {
      posix_file_descriptors = new std::unordered_map<int, const char *>();
    }
    posix_file_descriptors->insert(std::make_pair(result, (const char *)traced_filename));
  }
  return result;
}

int (*orig___open)(const char *pathname, int flags, int mode);
int fake___open(const char *pathname, int flags, int mode) {
  char *traced_filename = NULL;
  if (pathname) {
    traced_filename = (char *)malloc(MAXPATHLEN);
    // FIXME: strncpy
    strcpy(traced_filename, pathname);
    LOG(1, "[-] trace open handle: ", pathname);
  }
  int result = orig___open(pathname, flags, mode);
  if (result != -1) {
    if (posix_file_descriptors == NULL) {
      posix_file_descriptors = new std::unordered_map<int, const char *>();
    }
    posix_file_descriptors->insert(std::make_pair(result, (const char *)traced_filename));
  }
  return result;
}

static const char *get_traced_filename(int fd, bool removed) {
  if (posix_file_descriptors == NULL)
    return NULL;
  std::unordered_map<int, const char *>::iterator it;
  it = posix_file_descriptors->find(fd);
  if (it != posix_file_descriptors->end()) {
    if (removed)
      posix_file_descriptors->erase(it);
    return it->second;
  }
  return NULL;
}

ssize_t (*orig_read)(int fd, void *buf, size_t count);
ssize_t fake_read(int fd, void *buf, size_t count) {
  const char *traced_filename = get_traced_filename(fd, false);
  if (traced_filename) {
    LOG(1, "[-] read: %s, buffer: %p, size: %zu", traced_filename, buf, count);
  }
  return orig_read(fd, buf, count);
}

ssize_t (*orig_write)(int fd, const void *buf, size_t count);
ssize_t fake_write(int fd, const void *buf, size_t count) {
  const char *traced_filename = get_traced_filename(fd, false);
  if (traced_filename) {
    LOG(1, "[-] write: %s, buffer: %p, size: %zu", traced_filename, buf, count);
  }
  return orig_write(fd, buf, count);
}
int (*orig_close)(int fd);
int fake_close(int fd) {
  const char *traced_filename = get_traced_filename(fd, true);
  if (traced_filename) {
    LOG(1, "[-] close: %s", traced_filename);
    free((void *)traced_filename);
  }
  return orig_close(fd);
}

#if 0
__attribute__((constructor)) static void ctor() {
  DobbyHook((void *)DobbySymbolResolver(NULL, "open"), (void *)fake_open, (void **)&orig_open);

  DobbyHook((void *)DobbySymbolResolver(NULL, "write"), (void *)fake_write, (void **)&orig_write);

  DobbyHook((void *)DobbySymbolResolver(NULL, "read"), (void *)fake_read, (void **)&orig_read);

  DobbyHook((void *)DobbySymbolResolver(NULL, "close"), (void *)fake_close, (void **)&orig_close);
}
#endif
