#ifndef CORE_ASSEMBLER_ARM_H
#define CORE_ASSEMBLER_ARM_H

#include "common_header.h"

#include "core/arch/arm/constants-arm.h"
#include "core/arch/arm/registers-arm.h"
#include "core/assembler/assembler.h"

#include "MemoryAllocator/CodeBuffer/code_buffer_arm.h"

enum ref_label_type_t { kLdrLiteral };

namespace zz {
namespace arm {

// ARM design had a 3-stage pipeline (fetch-decode-execute)
#define ARM_PC_OFFSET 8
#define Thumb_PC_OFFSET 4

// define instruction length
#define ARM_INST_LEN 4
#define Thumb1_INST_LEN 2
#define Thumb2_INST_LEN 4

// Thumb instructions address is odd
#define THUMB_ADDRESS_FLAG 1

constexpr Register TMP_REG_0 = r12;

constexpr Register VOLATILE_REGISTER = r12;

#define Rd(rd) (rd.code() << kRdShift)
#define Rt(rt) (rt.code() << kRtShift)
#define Rn(rn) (rn.code() << kRnShift)
#define Rm(rm) (rm.code() << kRmShift)

// ---

class Operand {
  friend class OpEncode;

public:
  Operand(int immediate) : imm_(immediate), rm_(no_reg), shift_(LSL), shift_imm_(0), rs_(no_reg) {
  }

  Operand(Register rm) : imm_(0), rm_(rm), shift_(LSL), shift_imm_(0), rs_(no_reg) {
  }

  Operand(Register rm, Shift shift, uint32_t shift_imm)
      : imm_(0), rm_(rm), shift_(shift), shift_imm_(shift_imm), rs_(no_reg) {
  }

  Operand(Register rm, Shift shift, Register rs) : imm_(0), rm_(rm), shift_(shift), shift_imm_(0), rs_(rs) {
  }

public:
  int GetImmediate() const {
    return imm_;
  }

private:
  Register rm_;
  Register rs_;

  Shift shift_;
  int shift_imm_;

  int imm_;

private:
  friend class EncodeUtility;
};

// ---

class MemOperand {
  friend class OpEncode;

public:
  MemOperand(Register rn, int32_t offset = 0, AddrMode addrmode = Offset)
      : rn_(rn), offset_(offset), rm_(no_reg), shift_(LSL), shift_imm_(0), addrmode_(addrmode) {
  }

  MemOperand(Register rn, Register rm, AddrMode addrmode = Offset)
      : rn_(rn), offset_(0), rm_(rm), shift_(LSL), shift_imm_(0), addrmode_(addrmode) {
  }

  MemOperand(Register rn, Register rm, Shift shift, uint32_t shift_imm, AddrMode addrmode = Offset)
      : rn_(rn), offset_(0), rm_(rm), shift_(shift), shift_imm_(shift_imm), addrmode_(addrmode) {
  }

  const Register &rn() const {
    return rn_;
  }
  const Register &rm() const {
    return rm_;
  }
  int32_t offset() const {
    return offset_;
  }

  bool IsImmediateOffset() const {
    return (addrmode_ == Offset);
  }
  bool IsRegisterOffset() const {
    return (addrmode_ == Offset);
  }
  bool IsPreIndex() const {
    return addrmode_ == PreIndex;
  }
  bool IsPostIndex() const {
    return addrmode_ == PostIndex;
  }

private:
  Register rn_; // base
  Register rm_; // register offset

  int32_t offset_; // valid if rm_ == no_reg

  Shift shift_;
  uint32_t shift_imm_; // valid if rm_ != no_reg && rs_ == no_reg

  AddrMode addrmode_; // bits P, U, and W
};

// ---

class OpEncode {
public:
  static uint32_t MemOperand(const MemOperand operand) {
    uint32_t encoding = 0;
    if (operand.rm_.IsValid()) {
      UNREACHABLE();
    }

    // sign
    uint32_t U = 0;
    if (operand.offset_ >= 0) {
      U = (1 << 23);
    }
    encoding |= U;

    // offset
    encoding |= bits(abs(operand.offset_), 0, 11);

    // addr mode
    uint32_t P, W;
    if (operand.addrmode_ == Offset) {
      P = 1;
      W = 0;
    } else if (operand.addrmode_ == PostIndex) {
      P = 0;
      W = 0;
    } else if (operand.addrmode_ == PreIndex) {
      P = 1;
      W = 1;
    }
    encoding |= ((P << 24) | (W << 21));

    // rn
    encoding |= Rn(operand.rn_);

    return encoding;
  }

  static uint32_t Operand(const Operand operand) {
    uint32_t encoding = 0;
    if (operand.rm_.IsValid()) {
      encoding = static_cast<uint32_t>(operand.rm_.code());
    } else {
      encoding = operand.GetImmediate();
    }

    return encoding;
  }
};

// ---

enum ExecuteState { ARMExecuteState, ThumbExecuteState };

class Assembler : public AssemblerBase {
private:
  ExecuteState execute_state_;

public:
  Assembler(void *address) : AssemblerBase(address) {
    execute_state_ = ARMExecuteState;
    buffer_ = new CodeBuffer();
  }

  // shared_ptr is better choice
  // but we can't use it at kernelspace
  Assembler(void *address, CodeBuffer *buffer) : AssemblerBase(address) {
    execute_state_ = ARMExecuteState;
    buffer_ = buffer;
  }

  void ClearCodeBuffer() {
    buffer_ = NULL;
  }

public:
  void SetExecuteState(ExecuteState state) {
    execute_state_ = state;
  }
  ExecuteState GetExecuteState() {
    return execute_state_;
  }

  void SetRealizedAddress(void *address) {
    DCHECK_EQ(0, reinterpret_cast<uint64_t>(address) % 4);
    AssemblerBase::SetRealizedAddress(address);
  }

  void EmitARMInst(arm_inst_t instr);

  void EmitAddress(uint32_t value);

public:
  void sub(Register rd, Register rn, const Operand &operand) {
    uint32_t encoding = B25 | B22;
    add_sub(encoding, AL, rd, rn, operand);
  }

  void add(Register rd, Register rn, const Operand &operand) {
    uint32_t encoding = B25 | B23;
    add_sub(encoding, AL, rd, rn, operand);
  }

  void add_sub(uint32_t encoding, Condition cond, Register rd, Register rn, const Operand &operand) {
    encoding |= (cond << kConditionShift);

    uint32_t imm = operand.GetImmediate();
    encoding |= imm;

    encoding |= Rd(rd);

    encoding |= Rn(rn);

    buffer_->EmitARMInst(encoding);
  }

  void ldr(Register rt, const MemOperand &operand) {
    uint32_t encoding = B20 | B26;
    load_store(encoding, AL, rt, operand);
  }

  void str(Register rt, const MemOperand &operand) {
    uint32_t encoding = B26;
    load_store(encoding, AL, rt, operand);
  }

  void load_store(uint32_t encoding, Condition cond, Register rt, const MemOperand &operand) {
    encoding |= (cond << kConditionShift);
    encoding |= Rt(rt) | OpEncode::MemOperand(operand);
    buffer_->EmitARMInst(encoding);
  }

  void mov(Register rd, const Operand &operand) {
    mov(AL, rd, operand);
  }

  void mov(Condition cond, Register rd, const Operand &operand) {
    uint32_t encoding = 0x01a00000;
    encoding |= (cond << kConditionShift);
    encoding |= Rd(rd) | OpEncode::Operand(operand);
    buffer_->EmitARMInst(encoding);
  }

  // Branch instructions.
  void b(int branch_offset) {
    b(AL, branch_offset);
  }
  void b(Condition cond, int branch_offset) {
    uint32_t encoding = 0xa000000;
    encoding |= (cond << kConditionShift);
    uint32_t imm24 = bits(branch_offset >> 2, 0, 23);
    encoding |= imm24;
    buffer_->EmitARMInst(encoding);
  }

  void bl(int branch_offset) {
    bl(AL, branch_offset);
  }
  void bl(Condition cond, int branch_offset) {
    uint32_t encoding = 0xb000000;
    encoding |= (cond << kConditionShift);
    uint32_t imm24 = bits(branch_offset >> 2, 0, 23);
    encoding |= imm24;
    buffer_->EmitARMInst(encoding);
  }

  void blx(int branch_offset) {
    UNIMPLEMENTED();
  }
  void blx(Register target, Condition cond = AL) {
    UNIMPLEMENTED();
  }
  void bx(Register target, Condition cond = AL) {
    UNIMPLEMENTED();
  }

}; // namespace arm

// ---

class TurboAssembler : public Assembler {
public:
  TurboAssembler(void *address) : Assembler(address) {
  }

  ~TurboAssembler() {
  }

  TurboAssembler(void *address, CodeBuffer *buffer) : Assembler(address, buffer) {
  }

  void Ldr(Register rt, AssemblerPseudoLabel *label) {
    if (label->relocated_pos()) {
      int offset = label->relocated_pos() - buffer_->GetBufferSize();
      ldr(rt, MemOperand(pc, offset));
    } else {
      // record this ldr, and fix later.
      label->link_to(kLdrLiteral, 0, buffer_->GetBufferSize());
      ldr(rt, MemOperand(pc, 0));
    }
  }

  void CallFunction(ExternalReference function) {
    // trick: use bl to replace lr register
    bl(0);
    b(4);
    ldr(pc, MemOperand(pc, -4));
    buffer_->Emit32((uint32_t)(uintptr_t)function.address());
  }

  void Move32Immeidate(Register rd, const Operand &x, Condition cond = AL) {
  }

  void RelocLabelFixup(tinystl::unordered_map<off_t, off_t> *relocated_offset_map) {
    for (auto *data_label : data_labels_) {
      auto val = data_label->data<int32_t>();
      auto iter = relocated_offset_map->find(val);
      if (iter != relocated_offset_map->end()) {
        data_label->fixup_data<int32_t>(iter->second);
      }
    }
  }
};

} // namespace arm
} // namespace zz

#endif
