#pragma once

struct x86_insn_t {};

struct x86_insn_decoder_t {
  uint8_t *buffer;
  uint32_t buffer_size;

  uint8_t mode = 64;

  x86_insn_t insn;
  explicit x86_insn_decoder_t(uint8_t *buffer, uint32_t buffer_size) : buffer(buffer), buffer_size(buffer_size) {
  }

  uint8_t peak_byte() const {
    return *buffer;
  }

  void decode_prefix() {
  }
};