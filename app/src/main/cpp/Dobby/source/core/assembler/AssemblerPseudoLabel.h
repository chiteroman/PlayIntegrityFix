#pragma once

#include "MemoryAllocator/CodeBuffer/CodeBufferBase.h"

class Label {
public:
  Label(addr_t addr) : pos_(addr) {
  }

protected:
  addr_t pos_;
};

class AssemblerPseudoLabel : public Label {
public:
  typedef struct {
    int link_type;
    size_t pc_offset;
    addr_t vmaddr_;
  } ref_label_insn_t;

public:
  AssemblerPseudoLabel(addr_t addr) : Label(addr) {
    ref_label_insns_.reserve(4);

    bind_to(addr);
  }

  bool has_confused_instructions() {
    return ref_label_insns_.size();
  }

  void link_confused_instructions();

  void link_confused_instructions(CodeBufferBase *buffer_);

  void link_to(int link_type, uint32_t pc_offset) {
    ref_label_insn_t insn;
    insn.link_type = link_type;
    insn.pc_offset = pc_offset;
    ref_label_insns_.push_back(insn);
  }

public:
  addr_t pos() {
    return pos_;
  };

  void bind_to(addr_t addr) {
    pos_ = addr;
  }

protected:
  tinystl::vector<ref_label_insn_t> ref_label_insns_;
};

struct RelocLabel : public AssemblerPseudoLabel {
public:
  RelocLabel() : AssemblerPseudoLabel(0) {
    memset(data_, 0, sizeof(data_));
    data_size_ = 0;
  }

  template <typename T> static RelocLabel *withData(T value) {
    auto label = new RelocLabel();
    label->setData(value);
    return label;
  }

  template <typename T> T data() {
    return *(T *)data_;
  }

  template <typename T> void setData(T value) {
    data_size_ = sizeof(T);
    memcpy(data_, &value, data_size_);
  }

  template <typename T> void fixupData(T value) {
    *(T *)data_ = value;
  }

  uint8_t data_[8];
  int data_size_;
};