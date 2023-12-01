#include "platform_detect_macro.h"
#if defined(TARGET_ARCH_IA32)

#include "dobby/dobby_internal.h"

#include "core/assembler/assembler-ia32.h"
#include "core/codegen/codegen-ia32.h"

#include "InstructionRelocation/x86/InstructionRelocationX86.h"

#include "MemoryAllocator/NearMemoryAllocator.h"
#include "InterceptRouting/RoutingPlugin/RoutingPlugin.h"

using namespace zz::x86;

CodeBufferBase *GenerateNormalTrampolineBuffer(addr_t from, addr_t to) {
  TurboAssembler turbo_assembler_((void *)from);
#define _ turbo_assembler_.

  CodeGen codegen(&turbo_assembler_);
  codegen.JmpNear((uint32_t)to);

  CodeBufferBase *result = NULL;
  result = turbo_assembler_.GetCodeBuffer()->Copy();
  return result;
}

CodeBufferBase *GenerateNearTrampolineBuffer(InterceptRouting *routing, addr_t src, addr_t dst) {
  DEBUG_LOG("x86 near branch trampoline enable default");
  return NULL;
}

#endif