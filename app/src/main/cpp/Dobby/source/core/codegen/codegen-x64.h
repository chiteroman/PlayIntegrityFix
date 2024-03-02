#ifndef CORE_CODEGEN_X64_H
#define CORE_CODEGEN_X64_H

#include "core/codegen/codegen.h"
#include "core/assembler/assembler.h"
#include "core/assembler/assembler-x64.h"

namespace zz {
namespace x64 {

class CodeGen : public CodeGenBase {
public:
  CodeGen(TurboAssembler *turbo_assembler) : CodeGenBase(turbo_assembler) {
  }

  void JmpNearIndirect(addr_t forward_stub_addr);
};

} // namespace x64
} // namespace zz

#endif