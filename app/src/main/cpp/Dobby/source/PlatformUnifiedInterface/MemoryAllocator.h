#pragma once

#include "common_header.h"

struct MemRange {
  addr_t start;
  addr_t end;
  size_t size;

  MemRange(addr_t start, size_t size) : start(start), end(0), size(size) {
    end = start + size;
  }

  void reset(addr_t start, size_t size) {
    this->start = start;
    this->size = size;
    end = start + size;
  }
};

struct MemBlock : MemRange {
  addr_t addr;

  MemBlock() : MemRange(0, 0), addr(0) {
  }

  MemBlock(addr_t addr, size_t size) : MemRange(addr, size), addr(addr) {
  }

  void reset(addr_t addr, size_t size) {
    MemRange::reset(addr, size);
    this->addr = addr;
  }
};

struct MemoryArena : MemRange {
  addr_t addr;
  addr_t cursor_addr;

  std::vector<MemBlock *> memory_blocks;

  MemoryArena(addr_t addr, size_t size) : MemRange(addr, size), addr(addr), cursor_addr(addr) {
  }

  virtual MemBlock *allocMemBlock(size_t size);
};

using CodeMemBlock = MemBlock;
using CodeMemoryArena = MemoryArena;

#if 0
struct CodeMemoryArena : MemoryArena {
  CodeMemoryArena(addr_t addr, size_t size) : MemoryArena(addr, size) {
  }

  CodeMemBlock *allocateCodeMemBlock(size_t size) {
    return allocMemBlock(size);
  }
};
#endif

using DataMemBlock = MemBlock;
using DataMemoryArena = MemoryArena;

#if 0
struct DataMemoryArena : MemoryArena {
public:
  DataMemoryArena(addr_t addr, size_t size) : MemoryArena(addr, size) {
  }

  DataMemBlock *allocateDataMemBlock(size_t size) {
    return allocMemBlock(size);
  }
};
#endif

class NearMemoryAllocator;
class MemoryAllocator {
  friend class NearMemoryAllocator;

private:
  std::vector<CodeMemoryArena *> code_arenas;
  std::vector<DataMemoryArena *> data_arenas;

private:
  static MemoryAllocator *shared_allocator;

public:
  static MemoryAllocator *SharedAllocator();

public:
  CodeMemoryArena *allocateCodeMemoryArena(uint32_t size);
  CodeMemBlock *allocateExecBlock(uint32_t size);
  uint8_t *allocateExecMemory(uint32_t size);
  uint8_t *allocateExecMemory(uint8_t *buffer, uint32_t buffer_size);

  DataMemoryArena *allocateDataMemoryArena(uint32_t size);
  DataMemBlock *allocateDataBlock(uint32_t size);
  uint8_t *allocateDataMemory(uint32_t size);
  uint8_t *allocateDataMemory(uint8_t *buffer, uint32_t buffer_size);
};