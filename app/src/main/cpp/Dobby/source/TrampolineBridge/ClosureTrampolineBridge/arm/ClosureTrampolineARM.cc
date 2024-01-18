#include "platform_macro.h"
#if defined(TARGET_ARCH_ARM)

#include "dobby_internal.h"

#include "core/assembler/assembler-arm.h"

#include "TrampolineBridge/ClosureTrampolineBridge/ClosureTrampoline.h"

using namespace zz;
using namespace zz::arm;

ClosureTrampolineEntry *ClosureTrampoline::CreateClosureTrampoline(void *carry_data, void *carry_handler) {
  ClosureTrampolineEntry *tramp_entry = nullptr;
  tramp_entry = new ClosureTrampolineEntry;
  
#ifdef ENABLE_CLOSURE_TRAMPOLINE_TEMPLATE
#define CLOSURE_TRAMPOLINE_SIZE (7 * 4)
  // use closure trampoline template code, find the executable memory and patch it.
  auto code = AssemblyCodeBuilder::FinalizeCodeFromAddress(closure_trampoline_template, CLOSURE_TRAMPOLINE_SIZE);
#else
// use assembler and codegen modules instead of template_code
#include "TrampolineBridge/ClosureTrampolineBridge/ClosureTrampoline.h"
#define _ turbo_assembler_.
  TurboAssembler turbo_assembler_(0);

  AssemblerPseudoLabel entry_label;
  AssemblerPseudoLabel forward_bridge_label;

  _ Ldr(r12, &entry_label);
  _ Ldr(pc, &forward_bridge_label);
  _ PseudoBind(&entry_label);
  _ EmitAddress((uint32_t)(uintptr_t)tramp_entry);
  _ PseudoBind(&forward_bridge_label);
  _ EmitAddress((uint32_t)(uintptr_t)get_closure_bridge());

  auto closure_tramp = AssemblyCodeBuilder::FinalizeFromTurboAssembler(&turbo_assembler_);
  tramp_entry->address = (void *)closure_tramp->addr;
  tramp_entry->size = closure_tramp->size;
  tramp_entry->carry_data = carry_data;
  tramp_entry->carry_handler = carry_handler;

  delete closure_tramp;

  return tramp_entry;
#endif
}

#endif