#pragma once

#include "InterceptEntry.h"
#include "MemoryAllocator/AssemblyCodeBuilder.h"
#include "InstructionRelocation/InstructionRelocation.h"
#include "TrampolineBridge/Trampoline/Trampoline.h"

class InterceptRouting {
public:
  explicit InterceptRouting(InterceptEntry *entry) : entry_(entry) {
    entry->routing = this;

    origin_ = nullptr;
    relocated_ = nullptr;

    trampoline_ = nullptr;
    trampoline_buffer_ = nullptr;
    trampoline_target_ = 0;
  }

  virtual void DispatchRouting() = 0;

  virtual void Prepare();

  virtual void Active();

  void Commit();

  InterceptEntry *GetInterceptEntry();

  void SetTrampolineBuffer(CodeBufferBase *buffer) {
    trampoline_buffer_ = buffer;
  }

  CodeBufferBase *GetTrampolineBuffer() {
    return trampoline_buffer_;
  }

  void SetTrampolineTarget(addr_t address) {
    trampoline_target_ = address;
  }

  addr_t GetTrampolineTarget() {
    return trampoline_target_;
  }

protected:
  bool GenerateRelocatedCode();

  bool GenerateTrampolineBuffer(addr_t src, addr_t dst);

protected:
  InterceptEntry *entry_;

  CodeMemBlock *origin_;
  CodeMemBlock *relocated_;

  CodeMemBlock *trampoline_;
  // trampoline buffer before active
  CodeBufferBase *trampoline_buffer_;
  addr_t trampoline_target_;
};
