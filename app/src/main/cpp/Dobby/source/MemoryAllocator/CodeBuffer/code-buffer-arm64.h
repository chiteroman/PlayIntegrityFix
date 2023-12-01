#ifndef CODE_BUFFER_ARM64_H
#define CODE_BUFFER_ARM64_H

#include "MemoryAllocator/CodeBuffer/CodeBufferBase.h"

typedef int32_t arm64_inst_t;

class CodeBuffer : public CodeBufferBase {

public:
  CodeBuffer() : CodeBufferBase() {
  }

public:
  arm64_inst_t LoadInst(uint32_t offset) {
    return *reinterpret_cast<int32_t *>(GetBuffer() + offset);
  }

  void RewriteInst(uint32_t offset, arm64_inst_t instr) {
    *reinterpret_cast<arm64_inst_t *>(GetBuffer() + offset) = instr;
  }
};

#endif
