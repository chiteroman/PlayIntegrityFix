#pragma once

#include "common_header.h"

class CodeBufferBase {
public:
  CodeBufferBase() {
  }

public:
  virtual CodeBufferBase *Copy();

  void Emit8(uint8_t data);

  void Emit16(uint16_t data);

  void Emit32(uint32_t data);

  void Emit64(uint64_t data);

  template <typename T> T Load(int offset) {
    return *(T *)(buffer_.data() + offset);
  }

  template <typename T> void Store(int offset, T value) {
    *(T *)(buffer_.data() + offset) = value;
  }

  template <typename T> void Emit(T value) {
    EmitBuffer((uint8_t *)&value, sizeof(value));
  }

  void EmitBuffer(uint8_t *buffer, int len);

  uint8_t *GetBuffer();
  size_t GetBufferSize();

private:
  tinystl::vector<uint8_t> buffer_;
};
