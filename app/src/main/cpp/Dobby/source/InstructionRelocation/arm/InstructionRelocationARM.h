#pragma once
#include "dobby/dobby_internal.h"

#include "core/arch/arm/constants-arm.h"
#include "core/assembler/assembler-arm.h"

namespace zz {
namespace arm {

// thumb1/thumb2 pseudo label type, only support Thumb1-Ldr | Thumb2-Ldr
enum ref_label_type_t { kThumb1Ldr, kThumb2LiteralLdr };

// custom thumb pseudo label for thumb/thumb2
class ThumbPseudoLabel : public AssemblerPseudoLabel {
public:
  ThumbPseudoLabel(addr_t addr) : AssemblerPseudoLabel(addr) {
  }

  // fix the instruction which not link to the label yet.
  void link_confused_instructions(CodeBuffer *buffer) {
    CodeBuffer *_buffer;
    if (buffer)
      _buffer = buffer;

    for (auto &ref_label_insn : ref_label_insns_) {
      // instruction offset to label
      thumb2_inst_t insn = _buffer->LoadThumb2Inst(ref_label_insn.pc_offset);
      thumb1_inst_t insn1 = _buffer->LoadThumb1Inst(ref_label_insn.pc_offset);
      thumb1_inst_t insn2 = _buffer->LoadThumb1Inst(ref_label_insn.pc_offset + sizeof(thumb1_inst_t));

      switch (ref_label_insn.link_type) {
      case kThumb1Ldr: {
        UNREACHABLE();
      } break;
      case kThumb2LiteralLdr: {
        int64_t pc = ref_label_insn.pc_offset + Thumb_PC_OFFSET;
        assert(pc % 4 == 0);
        int32_t imm12 = pos() - pc;

        if (imm12 > 0) {
          set_bit(insn1, 7, 1);
        } else {
          set_bit(insn1, 7, 0);
          imm12 = -imm12;
        }
        set_bits(insn2, 0, 11, imm12);
        _buffer->RewriteThumb1Inst(ref_label_insn.pc_offset, insn1);
        _buffer->RewriteThumb1Inst(ref_label_insn.pc_offset + Thumb1_INST_LEN, insn2);

        DEBUG_LOG("[thumb label link] insn offset %d link offset %d", ref_label_insn.pc_offset, imm12);
      } break;
      default:
        UNREACHABLE();
        break;
      }
    }
  }
};

class ThumbRelocLabelEntry : public ThumbPseudoLabel, public RelocLabel {
public:
  ThumbRelocLabelEntry(bool is_pc_register) : RelocLabel(), ThumbPseudoLabel(0), is_pc_register_(is_pc_register) {
  }

  template <typename T> static ThumbRelocLabelEntry *withData(T value, bool is_pc_register) {
    auto label = new ThumbRelocLabelEntry(is_pc_register);
    label->setData(value);
    return label;
  }

  bool is_pc_register() {
    return is_pc_register_;
  }

private:
  bool is_pc_register_;
};

// ---

class ThumbAssembler : public Assembler {
public:
  ThumbAssembler(void *address) : Assembler(address) {
    this->SetExecuteState(ThumbExecuteState);
  }

  ThumbAssembler(void *address, CodeBuffer *buffer) : Assembler(address, buffer) {
    this->SetExecuteState(ThumbExecuteState);
  }

  void EmitInt16(int16_t val) {
    buffer_->Emit16(val);
  }

  void Emit2Int16(int16_t val1, int16_t val2) {
    EmitInt16(val1);
    EmitInt16(val2);
  }

  void EmitAddress(uint32_t value) {
    buffer_->Emit32(value);
  }

  // =====
  void t1_nop() {
    EmitInt16(0xbf00);
  }
  void t1_b(int32_t imm) {
    ASSERT(CheckSignLength(imm, 12));
    ASSERT(CheckAlign(imm, 2));

    int32_t imm11 = bits(imm >> 1, 0, 10);
    EmitInt16(0xe000 | imm11);
  }

  // =====
  void t2_b(uint32_t imm) {
    EmitThumb2Branch(AL, imm, false);
  }
  void t2_bl(uint32_t imm) {
    EmitThumb2Branch(AL, imm, true);
  }
  void t2_blx(uint32_t imm) {
    UNIMPLEMENTED();
  }

  // =====
  void t2_ldr(Register dst, const MemOperand &src) {
    // WARNNING: literal ldr, base = ALIGN(pc, 4)
    EmitThumb2LoadStore(true, dst, src);
  }

private:
  void EmitThumb2LoadLiteral(Register rt, const MemOperand x) {
    bool add = true;
    uint32_t U, imm12;
    int32_t offset = x.offset();

#if 0
    // literal ldr, base = ALIGN(pc, 4)
    if (rt.Is(pc)) {
      // TODO: convert to `GetRealizedAddress()` ???
      addr_t curr_pc = pc_offset() + (addr_t)GetRealizedAddress();
      if (curr_pc % 4) {
        t1_nop();
      }
    }
#endif

    if (offset > 0) {
      U = B7;
      imm12 = offset;
    } else {
      U = 0;
      imm12 = -offset;
    }
    EmitInt16(0xf85f | U);
    EmitInt16(0x0 | (rt.code() << 12) | imm12);
  }
  void EmitThumb2LoadStore(bool load, Register rt, const MemOperand x) {
    if (x.rn().Is(pc)) {
      EmitThumb2LoadLiteral(rt, x);
      return;
    }

    bool index, add, wback;
    if (x.IsRegisterOffset() && x.offset() >= 0) {
      index = true, add = true, wback = false;
      uint32_t imm12 = x.offset();
      EmitInt16(0xf8d0 | (x.rn().code() << 0));
      EmitInt16(0x0 | (rt.code() << 12) | imm12);
    } else {
      // use bit accelerate
      uint32_t P = 0, W = 0, U = 0;
      uint32_t imm8 = x.offset() > 0 ? x.offset() : -x.offset();
      U = x.offset() > 0 ? 0 : B9;
      if (x.IsPostIndex()) {
        P = 0, W = B8;
      } else if (x.IsPreIndex()) {
        P = B10, W = B8;
      }
      index = (P == B10);
      add = (U == B9);
      wback = (W == B8);
      EmitInt16(0xf850 | (x.rn().code() << 0));
      EmitInt16(0x0800 | (rt.code() << 12) | P | U | W | imm8);
    }
  }

  void EmitThumb2Branch(Condition cond, int32_t imm, bool link) {
    uint32_t operand = imm >> 1;
    ASSERT(CheckSignLength(operand, 25));
    ASSERT(CheckAlign(operand, 2));

    uint32_t signbit = (imm >> 31) & 0x1;
    uint32_t i1 = (operand >> 22) & 0x1;
    uint32_t i2 = (operand >> 21) & 0x1;
    uint32_t imm10 = (operand >> 11) & 0x03ff;
    uint32_t imm11 = operand & 0x07ff;
    uint32_t j1 = (!(i1 ^ signbit));
    uint32_t j2 = (!(i2 ^ signbit));

    if (cond != AL) {
      UNIMPLEMENTED();
    }

    EmitInt16(0xf000 | LeftShift(signbit, 1, 10) | LeftShift(imm10, 10, 0));
    if (link) {
      // Not use LeftShift(1, 1, 14), and use B14 for accelerate
      EmitInt16(0x9000 | LeftShift(j1, 1, 13) | (LeftShift(j2, 1, 11)) | LeftShift(imm11, 11, 0) | B14);
    } else {
      EmitInt16(0x9000 | LeftShift(j1, 1, 13) | (LeftShift(j2, 1, 11)) | LeftShift(imm11, 11, 0));
    }
  }
};

// ---

class ThumbTurboAssembler : public ThumbAssembler {
public:
  ThumbTurboAssembler(void *address) : ThumbAssembler(address) {
  }

  ThumbTurboAssembler(void *address, CodeBuffer *buffer) : ThumbAssembler(address, buffer) {
  }

  ~ThumbTurboAssembler() {
  }

  void T1_Ldr(Register rt, ThumbPseudoLabel *label) {
    UNREACHABLE();

// t1_ldr: rt can't be PC register
// ===
#if 0
    if (label->is_bound()) {
      const int64_t dest = label->pos() - buffer_.Size();
      ldr(rt, MemOperand(pc, dest));
    } else {
      // record this ldr, and fix later.
      label->link_to(buffer_.Size(), ThumbPseudoLabel::kThumb1Ldr);
      ldr(rt, MemOperand(pc, 0));
    }
#endif
  }

  void T2_Ldr(Register rt, ThumbPseudoLabel *label) {
    if (label->pos()) {
      int offset = label->pos() - buffer_->GetBufferSize();
      t2_ldr(rt, MemOperand(pc, offset));
    } else {
      // record this ldr, and fix later.
      label->link_to(kThumb2LiteralLdr, buffer_->GetBufferSize());
      t2_ldr(rt, MemOperand(pc, 0));
    }
  }

  void AlignThumbNop() {
    addr32_t pc = this->GetCodeBuffer()->GetBufferSize() + (uintptr_t)GetRealizedAddress();
    if (pc % Thumb2_INST_LEN) {
      t1_nop();
    } else {
    }
  }

  // ---

  void PseudoBind(ThumbPseudoLabel *label) {
    const addr_t bound_pc = buffer_->GetBufferSize();
    label->bind_to(bound_pc);
    // If some instructions have been wrote, before the label bound, we need link these `confused` instructions
    if (label->has_confused_instructions()) {
      label->link_confused_instructions(GetCodeBuffer());
    }
  }

  void RelocBind() {
    for (auto *data_label : data_labels_) {
      PseudoBind(data_label);
      reinterpret_cast<CodeBufferBase *>(buffer_)->EmitBuffer(data_label->data_, data_label->data_size_);
    }
  }

  void AppendRelocLabel(ThumbRelocLabelEntry *label) {
    data_labels_.push_back(label);
  }

  void RelocLabelFixup(tinystl::unordered_map<off_t, off_t> *relocated_offset_map) {
    for (auto *data_label : data_labels_) {
      auto val = data_label->data<int32_t>();
      auto iter = relocated_offset_map->find(val);
      if (iter != relocated_offset_map->end()) {
        data_label->fixupData<int32_t>(iter->second);
      }
    }
  }

private:
  tinystl::vector<ThumbRelocLabelEntry *> data_labels_;
};

#if 0
void GenRelocateCodeAndBranch(void *buffer, CodeMemBlock *origin, CodeMemBlock *relocated);
#endif

} // namespace arm
} // namespace zz
