#include "platform_macro.h"
#if defined(TARGET_ARCH_IA32)

#include "core/codegen/codegen-ia32.h"

namespace zz {
namespace x86 {

void CodeGen::JmpNear(uint32_t address) {
  TurboAssembler *turbo_assembler_ = reinterpret_cast<TurboAssembler *>(this->assembler_);
#define _ turbo_assembler_->
#define __ turbo_assembler_->GetCodeBuffer()->
  uint32_t currIP = turbo_assembler_->CurrentIP() + 5;
  int32_t offset = (int32_t)(address - currIP);

  __ Emit8(0xe9);
  __ Emit32(offset);
}

} // namespace x86
} // namespace zz

#endif