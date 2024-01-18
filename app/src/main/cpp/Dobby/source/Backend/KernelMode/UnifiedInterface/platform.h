#ifndef PLATFORM_INTERFACE_COMMON_PLATFORM_H
#define PLATFORM_INTERFACE_COMMON_PLATFORM_H

#include "common_header.h"

// ================================================================
// base :: OSMemory

enum MemoryPermission { kNoAccess, kRead, kReadWrite, kReadWriteExecute, kReadExecute };

class OSMemory {
public:
  static int PageSize();

  static void *Allocate(size_t size, MemoryPermission access);

  static void *Allocate(size_t size, MemoryPermission access, void *fixed_address);

  static bool Free(void *address, size_t size);

  static bool Release(void *address, size_t size);

  static bool SetPermission(void *address, size_t size, MemoryPermission access);
};

#endif
