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
    int type_;
    vmaddr_t vmaddr_;
    off_t offset_;
  } ref_label_inst_t;

public:
  AssemblerPseudoLabel() : AssemblerPseudoLabel(0) {
  }

  AssemblerPseudoLabel(vmaddr_t addr) : Label(addr) {
    ref_label_insts_.reserve(4);

    pos_ = addr;
    relocated_pos_ = 0;
  }

  ~AssemblerPseudoLabel(void) {
  }

  void link_confused_instructions(CodeBufferBase *buffer);

  bool has_confused_instructions() {
    return ref_label_insts_.size();
  }

  void link_confused_instructions();

  void link_to(int type, vmaddr_t inst_vmaddr, off_t offset) {
    ref_label_inst_t inst;
    inst.type_ = type;
    inst.vmaddr_ = inst_vmaddr;
    inst.offset_ = offset;
    ref_label_insts_.push_back(inst);
  }

private:
  addr_t relocated_pos_;

public:
  addr_t relocated_pos() {
    return relocated_pos_;
  };

  void bind_to(vmaddr_t addr) {
    relocated_pos_ = addr;
  }

  void relocate_to(vmaddr_t addr) {
    bind_to(addr);
  }

protected:
  std::vector<ref_label_inst_t> ref_label_insts_;
};

// ---

struct RelocLabel : public AssemblerPseudoLabel {
public:
  RelocLabel() : AssemblerPseudoLabel(0) {
  }

  template <typename T> RelocLabel(T value) : AssemblerPseudoLabel(0) {
    *(T *)data_ = value;
    data_size_ = sizeof(value);
  }

  template <typename T> T data() {
    return *(T *)data_;
  }

  template <typename T> void fixup_data(T value) {
    *(T *)data_ = value;
  }

  uint8_t data_[8];
  int data_size_;
};