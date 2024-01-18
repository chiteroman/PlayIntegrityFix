#include "platform_macro.h"
#if defined(TARGET_ARCH_ARM64)

#include "dobby_internal.h"

#include "core/assembler/assembler-arm64.h"

#include "TrampolineBridge/ClosureTrampolineBridge/ClosureTrampoline.h"

using namespace zz;
using namespace zz::arm64;

// // tips
// _ ldr(TMP_REG_1, OFFSETOF(ClosureTrampolineEntry, carry_data));
// _ ldr(TMP_REG_0, OFFSETOF(ClosureTrampolineEntry, carry_handler));

// use assembler and codegen modules instead of template_code
ClosureTrampolineEntry *ClosureTrampoline::CreateClosureTrampoline(void *carry_data, void *carry_handler) {
  ClosureTrampolineEntry *tramp_entry = nullptr;
  tramp_entry = new ClosureTrampolineEntry;

#define _ turbo_assembler_.
  TurboAssembler turbo_assembler_(0);

  AssemblerPseudoLabel entry_label;
  AssemblerPseudoLabel forward_bridge_label;

  // prologue: alloc stack, store lr
  _ sub(SP, SP, 2 * 8);
  _ str(x30, MemOperand(SP, 8));

  // store data at stack
  _ Ldr(TMP_REG_0, &entry_label);
  _ str(TMP_REG_0, MemOperand(SP, 0));

  _ Ldr(TMP_REG_0, &forward_bridge_label);
  _ blr(TMP_REG_0);

  // epilogue: release stack(won't restore lr)
  _ ldr(x30, MemOperand(SP, 8));
  _ add(SP, SP, 2 * 8);

  // branch to next hop
  _ br(TMP_REG_0);

  _ PseudoBind(&entry_label);
  _ EmitInt64((uint64_t)tramp_entry);
  _ PseudoBind(&forward_bridge_label);
  _ EmitInt64((uint64_t)get_closure_bridge());

  auto closure_tramp = AssemblyCodeBuilder::FinalizeFromTurboAssembler(static_cast<AssemblerBase *>(&turbo_assembler_));

  tramp_entry->address = (void *)closure_tramp->addr;
  tramp_entry->size = closure_tramp->size;
  tramp_entry->carry_data = carry_data;
  tramp_entry->carry_handler = carry_handler;

  delete closure_tramp;

  return tramp_entry;
}

#endif
