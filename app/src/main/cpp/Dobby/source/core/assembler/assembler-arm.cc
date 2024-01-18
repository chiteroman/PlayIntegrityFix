#include "platform_macro.h"
#if TARGET_ARCH_ARM

#include "core/assembler/assembler-arm.h"

void AssemblerPseudoLabel::link_confused_instructions(CodeBufferBase *buffer) {
  CodeBuffer *_buffer = (CodeBuffer *)buffer;

  for (auto &ref_label_inst : ref_label_insts_) {
    arm_inst_t inst = _buffer->LoadARMInst(ref_label_inst.offset_);
    if (ref_label_inst.type_ == kLdrLiteral) {
      int64_t pc = ref_label_inst.offset_ + ARM_PC_OFFSET;
      assert(pc % 4 == 0);
      int32_t imm12 = relocated_pos() - pc;
      if (imm12 > 0) {
        set_bit(inst, 23, 1);
      } else {
        set_bit(inst, 23, 0);
        imm12 = -imm12;
      }
      set_bits(inst, 0, 11, imm12);
    }
    _buffer->RewriteARMInst(ref_label_inst.offset_, inst);
  }
}

namespace zz {
namespace arm {

void Assembler::EmitARMInst(arm_inst_t instr) {
  buffer_->EmitARMInst(instr);
}

void Assembler::EmitAddress(uint32_t value) {
  buffer_->Emit32(value);
}

} // namespace arm
} // namespace zz

#endif
