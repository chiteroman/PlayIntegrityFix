#ifndef USER_MODE_PLATFORM_THREAD_H
#define USER_MODE_PLATFORM_THREAD_H

#include "common_header.h"

namespace zz {

class OSThread {
public:
  typedef int LocalStorageKey;

  static int GetCurrentProcessId();

  static int GetCurrentThreadId();

  // Thread-local storage.
  static LocalStorageKey CreateThreadLocalKey();

  static void DeleteThreadLocalKey(LocalStorageKey key);

  static void *GetThreadLocal(LocalStorageKey key);

  static int GetThreadLocalInt(LocalStorageKey key);

  static void SetThreadLocal(LocalStorageKey key, void *value);

  static void SetThreadLocalInt(LocalStorageKey key, int value);

  static bool HasThreadLocal(LocalStorageKey key);

  static void *GetExistingThreadLocal(LocalStorageKey key);
};

} // namespace zz

#endif