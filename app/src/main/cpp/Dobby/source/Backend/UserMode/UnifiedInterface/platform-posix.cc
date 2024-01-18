#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <assert.h>

#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#include <pthread_np.h> // for pthread_set_name_np
#endif

#include <sched.h> // for sched_yield
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/syscall.h>

#if defined(__APPLE__) || defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/sysctl.h> // NOLINT, for sysctl
#endif

#include "logging/logging.h"
#include "logging/check_logging.h"
#include "UnifiedInterface/platform.h"

#if defined(__APPLE__)
#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#endif

#if defined(ANDROID) && !defined(ANDROID_LOG_STDOUT)
#define ANDROID_LOG_TAG "Dobby"

#include <android/log.h>

#endif

#include <string.h>

#if defined(__APPLE__)
const int kMmapFd = VM_MAKE_TAG(255);
#else
const int kMmapFd = -1;
#endif

const int kMmapFdOffset = 0;

// ================================================================
// base :: Thread

using namespace base;

typedef struct thread_handle_t {
  pthread_t thread;
} thread_handle_t;

void ThreadInterface::SetName(const char *name) {
#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__OpenBSD__)
  pthread_set_name_np(pthread_self(), name);
#elif defined(__APPLE__)
  pthread_setname_np(name);
#endif
}

int ThreadInterface::CurrentId() {
#if defined(__APPLE__)
  mach_port_t port = mach_thread_self();
  mach_port_deallocate(mach_task_self(), port);
  return port;
#elif defined(_POSIX_VERSION)
  return syscall(__NR_gettid);
#endif
}

static void *thread_handler_wrapper(void *ctx) {
  ThreadInterface::Delegate *d = (ThreadInterface::Delegate *)ctx;
  d->ThreadMain();
  return nullptr;
}

bool ThreadInterface::Create(ThreadInterface::Delegate *delegate, ThreadHandle *handle) {
  thread_handle_t *handle_impl = new thread_handle_t;

  int err = 0;
  err = pthread_create(&(handle_impl->thread), nullptr, thread_handler_wrapper, delegate);
  if (err != 0) {
    FATAL("pthread create failed");
    return false;
  }
  return true;
}

Thread::Thread(const char *name) {
  strncpy(name_, name, sizeof(name_));
}

bool Thread::Start() {
  if (ThreadInterface::Create(this, &handle_) == false) {
    return false;
  }
  return true;
}

// --- OSMemory

static int GetProtectionFromMemoryPermission(MemoryPermission access) {
  switch (access) {
  case MemoryPermission::kNoAccess:
    return PROT_NONE;
  case MemoryPermission::kRead:
    return PROT_READ;
  case MemoryPermission::kReadWrite:
    return PROT_READ | PROT_WRITE;
  case MemoryPermission::kReadWriteExecute:
    return PROT_READ | PROT_WRITE | PROT_EXEC;
  case MemoryPermission::kReadExecute:
    return PROT_READ | PROT_EXEC;
  }
  UNREACHABLE();
}

int OSMemory::PageSize() {
  return static_cast<int>(sysconf(_SC_PAGESIZE));
}

void *OSMemory::Allocate(size_t size, MemoryPermission access) {
  return OSMemory::Allocate(size, access, nullptr);
}

void *OSMemory::Allocate(size_t size, MemoryPermission access, void *fixed_address) {
  int prot = GetProtectionFromMemoryPermission(access);

  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  if (fixed_address != nullptr) {
    flags = flags | MAP_FIXED;
  }
  void *result = mmap(fixed_address, size, prot, flags, kMmapFd, kMmapFdOffset);
  if (result == MAP_FAILED)
    return nullptr;

  return result;
}

bool OSMemory::Free(void *address, size_t size) {
  DCHECK_EQ(0, reinterpret_cast<uintptr_t>(address) % PageSize());
  DCHECK_EQ(0, size % PageSize());

  return munmap(address, size) == 0;
}

bool OSMemory::Release(void *address, size_t size) {
  DCHECK_EQ(0, reinterpret_cast<uintptr_t>(address) % PageSize());
  DCHECK_EQ(0, size % PageSize());

  return munmap(address, size) == 0;
}

bool OSMemory::SetPermission(void *address, size_t size, MemoryPermission access) {
  DCHECK_EQ(0, reinterpret_cast<uintptr_t>(address) % PageSize());
  DCHECK_EQ(0, size % PageSize());

  int prot = GetProtectionFromMemoryPermission(access);
  int ret = mprotect(address, size, prot);
  if (ret) {
    ERROR_LOG("OSMemory::SetPermission: %s\n", ((const char *)strerror(errno)));
  }

  return ret == 0;
}

// --- OSPrint

void OSPrint::Print(const char *format, ...) {
  va_list args;
  va_start(args, format);
  VPrint(format, args);
  va_end(args);
}

void OSPrint::VPrint(const char *format, va_list args) {
#if defined(ANDROID) && !defined(ANDROID_LOG_STDOUT)
  __android_log_vprint(ANDROID_LOG_INFO, ANDROID_LOG_TAG, format, args);
#else
  vprintf(format, args);
#endif
}

void OSPrint::PrintError(const char *format, ...) {
  va_list args;
  va_start(args, format);
  VPrintError(format, args);
  va_end(args);
}

void OSPrint::VPrintError(const char *format, va_list args) {
#if defined(ANDROID) && !defined(ANDROID_LOG_STDOUT)
  __android_log_vprint(ANDROID_LOG_ERROR, LOG_TAG, format, args);
#else
  vfprintf(stderr, format, args);
#endif
}
