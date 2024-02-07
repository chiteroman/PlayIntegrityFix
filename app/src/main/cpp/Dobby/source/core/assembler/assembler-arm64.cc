#include "platform_detect_macro.h"
#if TARGET_ARCH_ARM64

#include "core/assembler/assembler-arm64.h"

void AssemblerPseudoLabel::link_confused_instructions(CodeBufferBase *buffer_) {
  auto buffer = (CodeBuffer *)buffer_;

  for (auto &ref_label_insn : ref_label_insns_) {
    int64_t fixup_offset = pos() - ref_label_insn.pc_offset;

    arm64_inst_t inst = buffer->LoadInst(ref_label_insn.pc_offset);
    arm64_inst_t new_inst = 0;

    if (ref_label_insn.link_type == kLabelImm19) {
      new_inst = encode_imm19_offset(inst, fixup_offset);
    }

    buffer->RewriteInst(ref_label_insn.pc_offset, new_inst);
  }
}

using namespace zz::arm64;

#endif
