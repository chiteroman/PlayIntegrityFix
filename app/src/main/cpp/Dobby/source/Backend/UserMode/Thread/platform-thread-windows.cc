#include "PlatformThread.h"

using namespace zz;

int OSThread::GetCurrentProcessId() {
  return 0;
}

int OSThread::GetCurrentThreadId() {
  return 0;
}

OSThread::LocalStorageKey OSThread::CreateThreadLocalKey() {
  return 0;
}

void OSThread::DeleteThreadLocalKey(LocalStorageKey key) {
}

void *OSThread::GetThreadLocal(LocalStorageKey key) {
  return NULL;
}

void OSThread::SetThreadLocal(LocalStorageKey key, void *value) {
}
