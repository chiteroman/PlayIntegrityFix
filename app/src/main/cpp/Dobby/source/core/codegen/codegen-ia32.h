#ifndef CORE_CODEGEN_X86_H
#define CORE_CODEGEN_X86_H

#include "core/codegen/codegen.h"
#include "core/assembler/assembler.h"
#include "core/assembler/assembler-ia32.h"

namespace zz {
namespace x86 {

class CodeGen : public CodeGenBase {
public:
  CodeGen(TurboAssembler *turbo_assembler) : CodeGenBase(turbo_assembler) {
  }

  void JmpNear(uint32_t address);
};

} // namespace x86
} // namespace zz

#endif