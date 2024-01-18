#pragma once

#include "MemoryAllocator/CodeBuffer/CodeBufferBase.h"

#include "AssemblerPseudoLabel.h"

class CodeBuffer;

namespace zz {

class ExternalReference {
public:
  explicit ExternalReference(void *address) : address_(address) {
#if defined(__APPLE__) && __arm64e__
#if __has_feature(ptrauth_calls)
    address_ = ptrauth_strip(address, ptrauth_key_asia);
#endif
#endif
  }

  const void *address();

private:
  const void *address_;
};

class AssemblerBase {
public:
  explicit AssemblerBase(void *address);

  ~AssemblerBase();

  size_t ip_offset() const;

  size_t pc_offset() const;

  CodeBuffer *GetCodeBuffer();

  void PseudoBind(AssemblerPseudoLabel *label);

  void RelocBind();

  void AppendRelocLabel(RelocLabel *label);

protected:
  std::vector<RelocLabel *> data_labels_;

public:
  virtual void *GetRealizedAddress();

  virtual void SetRealizedAddress(void *address);

  static void FlushICache(addr_t start, int size);

  static void FlushICache(addr_t start, addr_t end);

protected:
  CodeBuffer *buffer_;

  void *realized_addr_;
};

} // namespace zz

#if 0
#include "globals.h"
#if TARGET_ARCH_ARM
#include "core/assembler/assembler-arm.h"
#elif TARGET_ARCH_ARM64
#include "core/assembler/assembler-arm64.h"
#elif TARGET_ARCH_X64
#include "core/assembler/assembler-x64.h"
#else
#error "unsupported architecture"
#endif
#endif