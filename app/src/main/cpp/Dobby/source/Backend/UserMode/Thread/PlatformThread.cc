#include "./PlatformThread.h"

namespace zz {
int OSThread::GetThreadLocalInt(LocalStorageKey key) {
  return static_cast<int>(reinterpret_cast<intptr_t>(GetThreadLocal(key)));
}

void OSThread::SetThreadLocalInt(LocalStorageKey key, int value) {
  SetThreadLocal(key, reinterpret_cast<void *>(static_cast<intptr_t>(value)));
}

bool OSThread::HasThreadLocal(LocalStorageKey key) {
  return GetThreadLocal(key) != nullptr;
}

void *OSThread::GetExistingThreadLocal(LocalStorageKey key) {
  return GetThreadLocal(key);
}
} // namespace zz