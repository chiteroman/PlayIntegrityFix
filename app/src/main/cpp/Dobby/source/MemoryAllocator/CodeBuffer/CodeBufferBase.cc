#include "MemoryAllocator/CodeBuffer/CodeBufferBase.h"

CodeBufferBase *CodeBufferBase::Copy() {
  CodeBufferBase *result = new CodeBufferBase();
  result->EmitBuffer(GetBuffer(), GetBufferSize());
  return result;
}

void CodeBufferBase::Emit8(uint8_t data) {
  Emit(data);
}

void CodeBufferBase::Emit16(uint16_t data) {
  Emit(data);
}

void CodeBufferBase::Emit32(uint32_t data) {
  Emit(data);
}

void CodeBufferBase::Emit64(uint64_t data) {
  Emit(data);
}

void CodeBufferBase::EmitBuffer(uint8_t *buffer, int buffer_size) {
  buffer_.insert(buffer_.end(), buffer, buffer + buffer_size);
}

uint8_t *CodeBufferBase::GetBuffer() {
  return buffer_.data();
}

size_t CodeBufferBase::GetBufferSize() {
  return buffer_.size();
}

#if 0 // Template Advanced won't enable even in userspace
template <typename T> T CodeBufferBase::Load(int offset) {
  return *reinterpret_cast<T *>(buffer + offset);
}

template <typename T> void CodeBufferBase::Store(int offset, T value) {
  *reinterpret_cast<T *>(buffer + offset) = value;
}

template <typename T> void CodeBufferBase::Emit(T value) {
  // Ensure the free space enough for the template T value
  ensureCapacity(sizeof(T) + GetBufferSize());

  *reinterpret_cast<T *>(buffer_cursor) = value;
  buffer_cursor += sizeof(T);
}
#endif
