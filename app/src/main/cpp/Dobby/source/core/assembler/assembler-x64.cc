#include "platform_macro.h"
#if defined(TARGET_ARCH_X64)

#include "core/assembler/assembler-x64.h"

using namespace zz::x64;

void AssemblerPseudoLabel::link_confused_instructions(CodeBufferBase *buffer) {
  CodeBuffer *_buffer = (CodeBuffer *)buffer;

  for (auto &ref_label_inst : ref_label_insts_) {
    int64_t new_offset = relocated_pos() - ref_label_inst.offset_;

    if (ref_label_inst.type_ == kDisp32_off_9) {
      // why 9 ?
      // use `call` and `pop` get the runtime ip register
      // but the ip register not the real call next insn
      // it need add two insn length == 9
      int disp32_fix_pos = ref_label_inst.offset_ - sizeof(int32_t);
      _buffer->FixBindLabel(disp32_fix_pos, new_offset + 9);
    }
  }
}

void Assembler::jmp(Immediate imm) {
  buffer_->Emit8(0xE9);
  buffer_->Emit32((int)imm.value());
}

addr64_t TurboAssembler::CurrentIP() {
  return pc_offset() + (addr_t)realized_addr_;
}

#endif