#include "logging/cxxlogging.h"

#if 1 || defined(BUILDING_KERNEL)
void Logger::setLogLevel(LogLevel level) {
  log_level_ = level;
}

void Logger::log(LogLevel level, const char *tag, const char *fmt, ...) {

}

void Logger::LogFatal(const char *fmt, ...) {
}
#else
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <vector>

void Logger::setLogLevel(LogLevel level) {
  log_level_ = level;
}

void Logger::log(LogLevel level, const char *tag, const char *fmt, ...) {
  if (level > log_level_) {
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
  }
}

void Logger::LogFatal(const char *fmt, ...) {
}
#endif
