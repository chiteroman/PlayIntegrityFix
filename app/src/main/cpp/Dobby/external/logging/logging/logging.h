#pragma once

#define LOG_TAG NULL

typedef enum {
  LOG_LEVEL_VERBOSE = 0,
  LOG_LEVEL_DEBUG = 1,
  LOG_LEVEL_INFO = 2,
  LOG_LEVEL_WARN = 3,
  LOG_LEVEL_ERROR = 4,
  LOG_LEVEL_FATAL = 5
} LogLevel;

#if 1
#ifdef __cplusplus
extern "C" {
#endif

void log_set_level(int level);

void log_set_tag(const char *tag);

void log_enable_time_tag();

void log_switch_to_syslog();

void log_switch_to_file(const char *path);

#if !defined(LOG_FUNCTION_IMPL)
#define LOG_FUNCTION_IMPL log_internal_impl
#endif

int log_internal_impl(int level, const char *, ...);

#if defined(LOGGING_DISABLE)
#define LOG_FUNCTION_IMPL(...)
#endif

#ifdef __cplusplus
}
#endif
#else
#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif
#endif

#define RAW_LOG(level, fmt, ...)                                                                                       \
  do {                                                                                                                 \
    LOG_FUNCTION_IMPL(level, fmt, ##__VA_ARGS__);                                                                      \
  } while (0)

#define LOG(level, fmt, ...)                                                                                           \
  do {                                                                                                                 \
    if (LOG_TAG)                                                                                                       \
      LOG_FUNCTION_IMPL(level, "[%s] " fmt, LOG_TAG, ##__VA_ARGS__);                                                   \
    else                                                                                                               \
      LOG_FUNCTION_IMPL(level, fmt, ##__VA_ARGS__);                                                                    \
  } while (0)

#define INFO_LOG(fmt, ...)                                                                                             \
  do {                                                                                                                 \
    LOG(LOG_LEVEL_INFO, "[*] " fmt, ##__VA_ARGS__);                                                                    \
  } while (0)

#define ERROR_LOG(fmt, ...)                                                                                            \
  do {                                                                                                                 \
    LOG(LOG_LEVEL_ERROR, "[!] [%s:%d:%s]" fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__);                           \
  } while (0)

#define FATAL(fmt, ...)                                                                                                \
  do {                                                                                                                 \
    LOG(LOG_LEVEL_ERROR, "[!] [%s:%d:%s]" fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__);                           \
    assert(0);                                                                                                         \
  } while (0)

#if defined(LOGGING_DEBUG)
#define DLOG(level, fmt, ...) LOG(level, fmt, ##__VA_ARGS__)
#else
#define DLOG(level, fmt, ...)
#endif

#define UNIMPLEMENTED() FATAL("%s\n", "unimplemented code!!!")
#define UNREACHABLE() FATAL("%s\n", "unreachable code!!!")