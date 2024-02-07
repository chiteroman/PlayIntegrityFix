#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define aync_logger_buffer_size (20 * 1024 * 1024)
int async_logger_buffer_cursor = 0;
char async_logger_buffer[aync_logger_buffer_size];

static pthread_mutex_t async_logger_mutex = PTHREAD_MUTEX_INITIALIZER;

static int output_fd = -1;

void async_logger_print(char *str) {
  pthread_mutex_lock(&async_logger_mutex);
#if 0
  {
    write(STDOUT_FILENO, str, strlen(str) + 1);
  }
#endif
  memcpy(async_logger_buffer + async_logger_buffer_cursor, str, strlen(str));
  async_logger_buffer_cursor += strlen(str);
  pthread_mutex_unlock(&async_logger_mutex);
  return;
}

static void *async_logger_print_impl(void *ctx) {
  while (1) {
    pthread_mutex_lock(&async_logger_mutex);
    if (async_logger_buffer_cursor > 0) {
      write(output_fd, async_logger_buffer, async_logger_buffer_cursor);
      async_logger_buffer_cursor = 0;
    }
    pthread_mutex_unlock(&async_logger_mutex);
    sleep(1);
  }
}

void async_logger_init(char *logger_path) {
  static int async_logger_initialized = 0;
  if (async_logger_initialized)
    return;
  async_logger_initialized = 1;

  // init stdout write lock
  pthread_mutex_t write_mutex;
  pthread_mutex_init(&write_mutex, NULL);

  output_fd = STDOUT_FILENO;
  if (logger_path) {
    int fd = open(logger_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    output_fd = fd;
  }

  // init async logger
  pthread_mutex_init(&async_logger_mutex, NULL);
  pthread_t async_logger_thread;
  int ret = pthread_create(&async_logger_thread, NULL, async_logger_print_impl, NULL);
}
