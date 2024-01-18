#include "platform_macro.h"
#if defined(TARGET_ARCH_IA32)

#include "MemoryAllocator/CodeBuffer/code-buffer-x86.h"

void CodeBuffer::Emit32(int32_t data) {
  ensureCapacity(GetBufferSize() + sizeof(int32_t));
  *reinterpret_cast<int32_t *>(getCursor()) = data;
  buffer_cursor += sizeof(int32_t);
  return;
}

void CodeBuffer::FixBindLabel(int offset, int32_t disp) {
  *reinterpret_cast<uint32_t *>(buffer + offset) = disp;
  return;
}

#endif