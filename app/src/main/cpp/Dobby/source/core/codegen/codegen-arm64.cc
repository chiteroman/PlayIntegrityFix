#include "platform_macro.h"
#if defined(TARGET_ARCH_ARM64)

#include "dobby_internal.h"
#include "core/codegen/codegen-arm64.h"

namespace zz {
namespace arm64 {

void CodeGen::LiteralLdrBranch(uint64_t address) {
  auto turbo_assembler_ = reinterpret_cast<TurboAssembler *>(this->assembler_);
#define _ turbo_assembler_->

  auto dst_label = new RelocLabel(address);
  turbo_assembler_->AppendRelocLabel(dst_label);

  _ Ldr(TMP_REG_0, dst_label);
  _ br(TMP_REG_0);

#undef _
}

} // namespace arm64
} // namespace zz

#endif
