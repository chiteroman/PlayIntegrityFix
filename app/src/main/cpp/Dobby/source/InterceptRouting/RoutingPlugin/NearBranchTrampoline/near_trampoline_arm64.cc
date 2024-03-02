#include "platform_detect_macro.h"
#if defined(TARGET_ARCH_ARM64)

#include "dobby/dobby_internal.h"

#include "core/assembler/assembler-arm64.h"
#include "core/codegen/codegen-arm64.h"

#include "MemoryAllocator/NearMemoryAllocator.h"
#include "InstructionRelocation/arm64/InstructionRelocationARM64.h"
#include "InterceptRouting/RoutingPlugin/RoutingPlugin.h"

using namespace zz::arm64;

#define ARM64_B_XXX_RANGE ((1 << 25) << 2) // signed

// If BranchType is B_Branch and the branch_range of `B` is not enough
// build the transfer to forward the b branch
static AssemblyCode *GenerateFastForwardTrampoline(addr_t src, addr_t dst) {
  TurboAssembler turbo_assembler_(nullptr);
#define _ turbo_assembler_.

  // [adrp + add + br branch]
  auto tramp_size = 3 * 4;
  auto tramp_mem = NearMemoryAllocator::SharedAllocator()->allocateNearExecMemory(tramp_size, src, ARM64_B_XXX_RANGE);
  if (tramp_mem == nullptr) {
    ERROR_LOG("search near code block failed");
    return nullptr;
  }

  // Use adrp + add branch
  uint64_t distance = llabs((int64_t)(tramp_mem - dst));
  uint64_t adrp_range = ((uint64_t)1 << (2 + 19 + 12 - 1));
  if (distance < adrp_range) {
    // use adrp + add + br branch == (3 * 4) trampoline size
    _ AdrpAdd(TMP_REG_0, (uint64_t)tramp_mem, dst);
    _ br(TMP_REG_0);
    DEBUG_LOG("forward trampoline use [adrp, add, br]");
  } else {
    // use mov + br == (4 * 5) trampoline size
    _ Mov(TMP_REG_0, dst);
    _ br(TMP_REG_0);
    DEBUG_LOG("forward trampoline use  [mov, br]");

    auto tramp_size = turbo_assembler_.GetCodeBuffer()->GetBufferSize();
    tramp_mem = NearMemoryAllocator::SharedAllocator()->allocateNearExecMemory(tramp_size, src, ARM64_B_XXX_RANGE);
    if (tramp_mem == nullptr) {
      ERROR_LOG("Can't found near code chunk");
      return nullptr;
    }
  }

  turbo_assembler_.SetRealizedAddress((void *)tramp_mem);

  AssemblyCode *code = nullptr;
  code = AssemblyCodeBuilder::FinalizeFromTurboAssembler(&turbo_assembler_);
  return code;
}

CodeBufferBase *GenerateNearTrampolineBuffer(InterceptRouting *routing, addr_t src, addr_t dst) {
  CodeBufferBase *result = nullptr;

  TurboAssembler turbo_assembler_((void *)src);
#define _ turbo_assembler_.

  // branch to trampoline_target directly
  if (llabs((long long)dst - (long long)src) < ARM64_B_XXX_RANGE) {
    _ b(dst - src);
  } else {
    auto fast_forward_trampoline = GenerateFastForwardTrampoline(src, dst);
    if (!fast_forward_trampoline)
      return nullptr;
    _ b(fast_forward_trampoline->addr - src);
  }

  // free the original trampoline
  result = turbo_assembler_.GetCodeBuffer()->Copy();
  return result;
}

#endif
