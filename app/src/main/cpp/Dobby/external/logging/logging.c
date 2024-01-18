#include "logging/logging.h"

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#if defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#endif

#if defined(__APPLE__)
#include <dlfcn.h>
#endif

#if defined(_WIN32)
#define PUBLIC
#else
#define PUBLIC __attribute__((visibility("default")))
#define INTERNAL __attribute__((visibility("internal")))
#endif

static int g_log_level = 1;
static char g_log_tag[64] = {0};
static bool time_tag_enabled = false;
static bool syslog_enabled = false;
static bool file_log_enabled = false;
static const char *log_file_path = NULL;
static int log_file_fd = -1;
static FILE *log_file_stream = NULL;

PUBLIC void log_set_level(int level) {
  g_log_level = level;
}

PUBLIC void log_set_tag(const char *tag) {
  sprintf(g_log_tag, "[%s] ", tag);
}

PUBLIC void log_enable_time_tag() {
  time_tag_enabled = true;
}

PUBLIC void log_switch_to_syslog(void) {
  syslog_enabled = 1;
}

PUBLIC void log_switch_to_file(const char *path) {
  file_log_enabled = 1;
  log_file_path = strdup(path);
  log_file_stream = fopen(log_file_path, "w+");
  if (log_file_stream == NULL) {
    file_log_enabled = false;
    ERROR_LOG("open log file %s failed, %s", path, strerror(errno));
  }
}

PUBLIC int log_internal_impl(int level, const char *fmt, ...) {
  if (level < g_log_level)
    return 0;

  char buffer[4096] = {0};

  if (g_log_tag[0] != 0) {
    snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "%s ", g_log_tag);
  }

  if (time_tag_enabled) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "%04d-%02d-%02d %02d:%02d:%02d ",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
  }

  snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "%s\n", fmt);

  fmt = buffer;

  va_list ap;
  va_start(ap, fmt);

#pragma clang diagnostic ignored "-Wformat"

  if (syslog_enabled) {
#if defined(__APPLE__)
    extern void *_os_log_default;
    static void (*os_log_with_args)(void *oslog, char type, const char *format, va_list args, void *ret_addr) = 0;
    if (!os_log_with_args)
      os_log_with_args = (__typeof(os_log_with_args))dlsym((void *)-2, "os_log_with_args");
    os_log_with_args(&_os_log_default, 0x10, fmt, ap, os_log_with_args);
#elif defined(_POSIX_VERSION)
    vsyslog(LOG_ERR, fmt, ap);
#endif
  }

  if (file_log_enabled) {
    char buffer[4096] = {0};
    vsnprintf(buffer, 4096 - 1, fmt, ap);
    if (fwrite(buffer, sizeof(char), strlen(buffer) + 1, log_file_stream) == -1) {
      file_log_enabled = false;
    }
    fflush(log_file_stream);
  }

  if (!syslog_enabled && !file_log_enabled) {
#if defined(__ANDROID__)
#define ANDROID_LOG_TAG "Dobby"
#include <android/log.h>
    __android_log_vprint(ANDROID_LOG_INFO, ANDROID_LOG_TAG, fmt, ap);
#else
    vprintf(fmt, ap);
#endif
  }

#pragma clang diagnostic warning "-Wformat"
  va_end(ap);
  return 0;
}
