#include "platform_detect_macro.h"
#if defined(TARGET_ARCH_X64)

#include "core/codegen/codegen-x64.h"

namespace zz {
namespace x64 {

void CodeGen::JmpNearIndirect(addr_t forward_stub_addr) {
  TurboAssembler *turbo_assembler_ = reinterpret_cast<TurboAssembler *>(this->assembler_);
#define _ turbo_assembler_->
#define __ turbo_assembler_->GetCodeBuffer()->
  uint64_t currIP = turbo_assembler_->CurrentIP() + 6;
  int32_t offset = (int32_t)(forward_stub_addr - currIP);

  // jmp *(rip + disp32)
  __ Emit8(0xFF);
  __ Emit8(0x25); // ModR/M: 00 100 101
  __ Emit32(offset);
}

} // namespace x64
} // namespace zz

#endif