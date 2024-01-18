/*
 * light weight pthread compatible library for Windows
 * (C) 2009 Okamura Yasunobu
 *
 *    WARNING This library does NOT support all future of pthread
 *
 */

#ifndef CROSS_THREAD_H
#define CROSS_THREAD_H

#ifdef _WIN32

#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>
#include <process.h>
#include <errno.h>

typedef struct pthread_tag {
  HANDLE handle;
} pthread_t;

typedef struct pthread_mutex_tag {
  HANDLE handle;
} pthread_mutex_t;

/* stub */
typedef struct pthread_attr_tag {
  int attr;
} pthread_attr_t;

typedef struct pthread_mutexattr_tag {
  int attr;
} pthread_mutexattr_t;

typedef DWORD pthread_key_t;

/* ignore attribute */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);

/* ignore value_ptr */
void pthread_exit(void *value_ptr);

/* ignore value_ptr */
int pthread_join(pthread_t thread, void **value_ptr);

pthread_t pthread_self(void);

/* do nothing */
int pthread_detach(pthread_t thread);

/* DO NOT USE */
int pthread_cancel(pthread_t thread);

int pthread_mutexattr_destroy(pthread_mutexattr_t *attr); /* do nothing */
int pthread_mutexattr_init(pthread_mutexattr_t *attr);    /* do nothing */

int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_trylock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

/* ignore deconstructor */
int pthread_key_create(pthread_key_t *key, void (*destr_function)(void *));
int pthread_key_delete(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void *pointer);
void *pthread_getspecific(pthread_key_t key);

#define sleep(num) Sleep(1000 * (num))

#ifdef __cplusplus
}
#endif

#else
#include <pthread.h>
#include <unistd.h>
#define Sleep(num) usleep(num * 1000)
#endif

#endif /* CROSS_THREAD_H */
