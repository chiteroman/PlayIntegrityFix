#ifndef PLATFORM_INTERFACE_COMMON_PLATFORM_H
#define PLATFORM_INTERFACE_COMMON_PLATFORM_H

#include <stdarg.h>
#include <stdint.h>

namespace base {
// ================================================================
// base :: ThreadLocalStorageInterface

class ThreadLocalStorageInterface {
  using LocalStorageKey = int32_t;

  // Thread-local storage.
  static LocalStorageKey CreateThreadLocalKey();

  static void DeleteThreadLocalKey(LocalStorageKey key);

  static void *GetThreadLocal(LocalStorageKey key);

  static int GetThreadLocalInt(LocalStorageKey key) {
    return static_cast<int>(reinterpret_cast<intptr_t>(GetThreadLocal(key)));
  }

  static void SetThreadLocal(LocalStorageKey key, void *value);

  static void SetThreadLocalInt(LocalStorageKey key, int value) {
    SetThreadLocal(key, reinterpret_cast<void *>(static_cast<intptr_t>(value)));
  }

  static bool HasThreadLocal(LocalStorageKey key) {
    return GetThreadLocal(key) != nullptr;
  }
};

// ================================================================
// base :: Thread

typedef void *ThreadHandle;

class ThreadInterface {
public:
  class Delegate {
  public:
    virtual void ThreadMain() = 0;
  };

public:
  static bool Create(Delegate *delegate, ThreadHandle *handle);

  static int CurrentId();

  static void SetName(const char *);
};

class Thread : public ThreadInterface, public ThreadInterface::Delegate {
public:
  Thread(const char *name);

  bool Start();

private:
  ThreadHandle handle_;

  char name_[256];
};
} // namespace base

// ================================================================
// base :: OSMemory

enum MemoryPermission { kNoAccess, kRead, kReadWrite, kReadWriteExecute, kReadExecute };

class OSMemory {
public:
  static int PageSize();

  static void *Allocate(size_t size, MemoryPermission access);

  static void *Allocate(size_t size, MemoryPermission access, void *fixed_address);

  static bool Free(void *address, size_t size);

  static bool Release(void *address, size_t size);

  static bool SetPermission(void *address, size_t size, MemoryPermission access);
};

// ================================================================
// base :: OSPrint

class OSPrint {
public:
  // Print output to console. This is mostly used for debugging output.
  // On platforms that has standard terminal output, the output
  // should go to stdout.
  static void Print(const char *format, ...);

  static void VPrint(const char *format, va_list args);

  // Print error output to console. This is mostly used for error message
  // output. On platforms that has standard terminal output, the output
  // should go to stderr.
  static void PrintError(const char *format, ...);

  static void VPrintError(const char *format, va_list args);
};

#endif
