#include "platform_macro.h"

#if defined(TARGET_ARCH_X64)

#include "dobby_internal.h"

#include "core/assembler/assembler-x64.h"
#include "core/codegen/codegen-x64.h"

#include "InstructionRelocation/x64/InstructionRelocationX64.h"

#include "MemoryAllocator/NearMemoryAllocator.h"
#include "InterceptRouting/RoutingPlugin/RoutingPlugin.h"

using namespace zz::x64;

static addr_t allocate_indirect_stub(addr_t jmp_insn_addr) {
  uint32_t jmp_near_range = (uint32_t)2 * 1024 * 1024 * 1024;
  auto stub_addr = (addr_t)NearMemoryAllocator::SharedAllocator()->allocateNearDataMemory(sizeof(void *), jmp_insn_addr,
                                                                                          jmp_near_range);
  if (stub_addr == 0) {
    ERROR_LOG("Not found near forward stub");
    return 0;
  }

  DLOG(0, "forward stub: %p", stub_addr);
  return stub_addr;
}

CodeBufferBase *GenerateNormalTrampolineBuffer(addr_t from, addr_t to) {
  TurboAssembler turbo_assembler_((void *)from);
#define _ turbo_assembler_.

  // allocate forward stub
  auto jump_near_next_insn_addr = from + 6;
  addr_t forward_stub = allocate_indirect_stub(jump_near_next_insn_addr);
  if (forward_stub == 0)
    return nullptr;

  *(addr_t *)forward_stub = to;

  CodeGen codegen(&turbo_assembler_);
  codegen.JmpNearIndirect((addr_t)forward_stub);

  auto buffer = turbo_assembler_.GetCodeBuffer()->Copy();
  return buffer;
}

CodeBufferBase *GenerateNearTrampolineBuffer(InterceptRouting *routing, addr_t src, addr_t dst) {
  DLOG(0, "x64 near branch trampoline enable default");
  return nullptr;
}

#endif