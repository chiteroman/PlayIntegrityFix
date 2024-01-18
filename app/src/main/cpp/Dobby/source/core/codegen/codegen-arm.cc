#include "platform_macro.h"
#if defined(TARGET_ARCH_ARM)

#include "core/codegen/codegen-arm.h"

namespace zz {
namespace arm {

void CodeGen::LiteralLdrBranch(uint32_t address) {
  TurboAssembler *turbo_assembler_ = reinterpret_cast<TurboAssembler *>(this->assembler_);
#define _ turbo_assembler_->
  _ ldr(pc, MemOperand(pc, -4));
  turbo_assembler_->GetCodeBuffer()->Emit32((addr_t)address);
}

} // namespace arm
} // namespace zz

#endif