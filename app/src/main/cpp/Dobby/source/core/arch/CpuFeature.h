#ifndef CORE_ARCH_CPU_FEATURE_H
#define CORE_ARCH_CPU_FEATURE_H

#include "common_header.h"

class CpuFeatures {
private:
  static void FlushICache(void *start, size_t size) {
    ClearCache(start, (void *)((addr_t)start + size));
  }

  static void FlushICache(void *start, void *end) {
    ClearCache(start, end);
  }

  static void ClearCache(void *start, void *end);
};

#endif
