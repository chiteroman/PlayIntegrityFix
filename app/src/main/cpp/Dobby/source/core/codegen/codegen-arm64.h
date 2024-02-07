#ifndef CORE_CODEGEN_ARM64_H
#define CORE_CODEGEN_ARM64_H

#include "core/codegen/codegen.h"
#include "core/assembler/assembler.h"
#include "core/assembler/assembler-arm64.h"

namespace zz {
namespace arm64 {

class CodeGen : public CodeGenBase {
public:
  CodeGen(TurboAssembler *turbo_assembler) : CodeGenBase(turbo_assembler) {
  }
  void LiteralLdrBranch(uint64_t address);
};

} // namespace arm64
} // namespace zz

#endif