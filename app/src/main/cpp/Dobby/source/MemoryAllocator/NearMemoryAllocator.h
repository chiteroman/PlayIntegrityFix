#pragma once

#include "PlatformUnifiedInterface/MemoryAllocator.h"

#include "dobby/common.h"

class NearMemoryAllocator {
public:
  MemoryAllocator *default_allocator;
  NearMemoryAllocator() {
    default_allocator = MemoryAllocator::SharedAllocator();
  }

private:
  static NearMemoryAllocator *shared_allocator;

public:
  static NearMemoryAllocator *SharedAllocator();

public:
  MemBlock *allocateNearBlock(uint32_t size, addr_t pos, size_t search_range, bool executable);
  MemBlock *allocateNearBlockFromDefaultAllocator(uint32_t size, addr_t pos, size_t search_range, bool executable);
  MemBlock *allocateNearBlockFromUnusedRegion(uint32_t size, addr_t pos, size_t search_range, bool executable);

  uint8_t *allocateNearExecMemory(uint32_t size, addr_t pos, size_t search_range);
  uint8_t *allocateNearExecMemory(uint8_t *buffer, uint32_t buffer_size, addr_t pos, size_t search_range);

  uint8_t *allocateNearDataMemory(uint32_t size, addr_t pos, size_t search_range);
  uint8_t *allocateNearDataMemory(uint8_t *buffer, uint32_t buffer_size, addr_t pos, size_t search_range);
};