#include "platform_detect_macro.h"
#if defined(TARGET_ARCH_ARM64)

#include "dobby/dobby_internal.h"
#include "core/codegen/codegen-arm64.h"

namespace zz {
namespace arm64 {

void CodeGen::LiteralLdrBranch(uint64_t address) {
  auto turbo_assembler_ = reinterpret_cast<TurboAssembler *>(this->assembler_);
#define _ turbo_assembler_->

  auto label = RelocLabel::withData(address);
  turbo_assembler_->AppendRelocLabel(label);

  _ Ldr(TMP_REG_0, label);
  _ br(TMP_REG_0);

#undef _
}

} // namespace arm64
} // namespace zz

#endif
