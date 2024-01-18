#include "platform_macro.h"
#if defined(TARGET_ARCH_X64) || defined(TARGET_ARCH_IA32)

#include "core/assembler/assembler-x86-shared.h"

using namespace zz::x86shared;

void Assembler::jmp(Immediate imm) {
  buffer_->Emit8(0xE9);
  buffer_->Emit32((int)imm.value());
}

uint64_t TurboAssembler::CurrentIP() {
  return pc_offset() + (addr_t)realized_addr_;
}

#endif