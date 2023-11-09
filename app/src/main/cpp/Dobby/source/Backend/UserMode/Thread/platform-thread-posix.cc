#include "Thread/PlatformThread.h"

#include <unistd.h>  // getpid
#include <pthread.h> // pthread
#include <sys/syscall.h>

using namespace zz;

int OSThread::GetCurrentProcessId() {
  return static_cast<int>(getpid());
}

int OSThread::GetCurrentThreadId() {
#if defined(__APPLE__)
  return static_cast<int>(pthread_mach_thread_np(pthread_self()));
#elif defined(__ANDROID__)
  return static_cast<int>(gettid());
#elif defined(__linux__)
  return static_cast<int>(syscall(__NR_gettid));
#else
  return static_cast<int>(reinterpret_cast<intptr_t>(pthread_self()));
#endif
}

static OSThread::LocalStorageKey PthreadKeyToLocalKey(pthread_key_t pthread_key) {
#if defined(__cygwin__)
  // We need to cast pthread_key_t to OSThread::LocalStorageKey in two steps
  // because pthread_key_t is a pointer type on Cygwin. This will probably not
  // work on 64-bit platforms, but Cygwin doesn't support 64-bit anyway.
  assert(sizeof(OSThread::LocalStorageKey) == sizeof(pthread_key_t));
  intptr_t ptr_key = reinterpret_cast<intptr_t>(pthread_key);
  return static_cast<OSThread::LocalStorageKey>(ptr_key);
#else
  return static_cast<OSThread::LocalStorageKey>(pthread_key);
#endif
}

static pthread_key_t LocalKeyToPthreadKey(OSThread::LocalStorageKey local_key) {
#if defined(__cygwin__)
  assert(sizeof(OSThread::LocalStorageKey) == sizeof(pthread_key_t));
  intptr_t ptr_key = static_cast<intptr_t>(local_key);
  return reinterpret_cast<pthread_key_t>(ptr_key);
#else
  return static_cast<pthread_key_t>(local_key);
#endif
}

OSThread::LocalStorageKey OSThread::CreateThreadLocalKey() {
  pthread_key_t key;
  int result = pthread_key_create(&key, nullptr);
  DCHECK_EQ(0, result);
  LocalStorageKey local_key = PthreadKeyToLocalKey(key);
  return local_key;
}

void OSThread::DeleteThreadLocalKey(LocalStorageKey key) {
  pthread_key_t pthread_key = LocalKeyToPthreadKey(key);
  int result = pthread_key_delete(pthread_key);
  DCHECK_EQ(0, result);
}

void *OSThread::GetThreadLocal(LocalStorageKey key) {
  pthread_key_t pthread_key = LocalKeyToPthreadKey(key);
  return pthread_getspecific(pthread_key);
}

void OSThread::SetThreadLocal(LocalStorageKey key, void *value) {
  pthread_key_t pthread_key = LocalKeyToPthreadKey(key);
  int result = pthread_setspecific(pthread_key, value);
  DCHECK_EQ(0, result);
}
