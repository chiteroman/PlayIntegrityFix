#include "core/assembler/assembler.h"
#include "logging/logging.h"

namespace zz {

const void *ExternalReference::address() {
  return address_;
}

AssemblerBase::AssemblerBase(void *address) {
  realized_addr_ = address;
  buffer_ = nullptr;
}

AssemblerBase::~AssemblerBase() {
  buffer_ = nullptr;
}

size_t AssemblerBase::ip_offset() const {
  return reinterpret_cast<CodeBufferBase *>(buffer_)->GetBufferSize();
}

size_t AssemblerBase::pc_offset() const {
  return reinterpret_cast<CodeBufferBase *>(buffer_)->GetBufferSize();
}

CodeBuffer *AssemblerBase::GetCodeBuffer() {
  return buffer_;
}

void AssemblerBase::PseudoBind(AssemblerPseudoLabel *label) {
  off_t bound_offset = reinterpret_cast<CodeBufferBase *>(buffer_)->GetBufferSize();
  label->bind_to(bound_offset);
  // If some instructions had been written, before the label bound, we need link these `confused` instructions
  if (label->has_confused_instructions()) {
    label->link_confused_instructions(reinterpret_cast<CodeBufferBase *>(buffer_));
  }
}

void AssemblerBase::RelocBind() {
  for (auto *data_label : data_labels_) {
    PseudoBind(data_label);
    reinterpret_cast<CodeBufferBase *>(buffer_)->EmitBuffer(data_label->data_, data_label->data_size_);
  }
}

void AssemblerBase::AppendRelocLabel(RelocLabel *label) {
  data_labels_.push_back(label);
}

void AssemblerBase::SetRealizedAddress(void *address) {
  realized_addr_ = address;
}

void *AssemblerBase::GetRealizedAddress() {
  return realized_addr_;
}

void AssemblerBase::FlushICache(addr_t start, int size) {
}

void AssemblerBase::FlushICache(addr_t start, addr_t end) {
}

} // namespace zz
