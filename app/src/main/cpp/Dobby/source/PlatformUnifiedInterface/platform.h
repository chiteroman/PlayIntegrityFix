#pragma once

#include "dobby/common.h"

namespace base {

class ThreadLocalStorageInterface {
  using LocalStorageKey = int32_t;

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
} // namespace base

class OSThread : public base::ThreadInterface, public base::ThreadInterface::Delegate {
  base::ThreadHandle handle_;

  char name_[256];

public:
  OSThread(const char *name);

  bool Start();
};

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

class OSPrint {
public:
  static void Print(const char *format, ...);

  static void VPrint(const char *format, va_list args);
};