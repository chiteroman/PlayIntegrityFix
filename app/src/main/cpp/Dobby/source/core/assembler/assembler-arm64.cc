#include "platform_macro.h"
#if TARGET_ARCH_ARM64

#include "core/assembler/assembler-arm64.h"

void AssemblerPseudoLabel::link_confused_instructions(CodeBufferBase *buffer) {
  CodeBuffer *_buffer = (CodeBuffer *)buffer;

  for (auto &ref_label_inst : ref_label_insts_) {
    int64_t fixup_offset = relocated_pos() - ref_label_inst.offset_;

    arm64_inst_t inst = _buffer->LoadInst(ref_label_inst.offset_);
    arm64_inst_t new_inst = 0;

    if (ref_label_inst.type_ == kLabelImm19) {
      new_inst = encode_imm19_offset(inst, fixup_offset);
    }

    _buffer->RewriteInst(ref_label_inst.offset_, new_inst);
  }
}

using namespace zz::arm64;

#endif
