#ifndef CODE_BUFFER_X64_H
#define CODE_BUFFER_X64_H

#include "MemoryAllocator/CodeBuffer/CodeBufferBase.h"

class CodeBuffer : public CodeBufferBase {
public:
  CodeBuffer() : CodeBufferBase() {
  }

public:
  void FixBindLabel(int offset, int32_t disp) {
    Store(offset, disp);
  }
};

#endif