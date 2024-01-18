#pragma once

#include "logging.h"

class Logger {
public:
  void setLogLevel(LogLevel level);

  void log(LogLevel level, const char *tag, const char *fmt, ...);

  void LogFatal(const char *fmt, ...);

private:
  LogLevel log_level_;
};