
#include "dobby/dobby_internal.h"

#include "TrampolineBridge/ClosureTrampolineBridge/ClosureTrampoline.h"

#include "InterceptRouting/Routing/InstructionInstrument/InstructionInstrumentRouting.h"
#include "InterceptRouting/Routing/InstructionInstrument/instrument_routing_handler.h"

// create closure trampoline jump to prologue_routing_dispatch with the `entry_` data
void InstructionInstrumentRouting::BuildRouting() {
  void *handler = (void *)instrument_routing_dispatch;
#if defined(__APPLE__) && defined(__arm64__)
  handler = pac_strip(handler);
#endif
  auto closure_trampoline = ClosureTrampoline::CreateClosureTrampoline(entry_, handler);
  this->SetTrampolineTarget((addr_t)closure_trampoline->address);
  DEBUG_LOG("[closure trampoline] closure trampoline: %p, data: %p", closure_trampoline->address, entry_);

  // generate trampoline buffer, before `GenerateRelocatedCode`
  addr_t from = entry_->patched_addr;
#if defined(TARGET_ARCH_ARM)
  if (entry_->thumb_mode)
    from += 1;
#endif
  addr_t to = GetTrampolineTarget();
  GenerateTrampolineBuffer(from, to);
}

void InstructionInstrumentRouting::DispatchRouting() {
  BuildRouting();

  // generate relocated code which size == trampoline size
  GenerateRelocatedCode();
}

#if 0
void *InstructionInstrumentRouting::GetTrampolineTarget() {
  return this->prologue_dispatch_bridge;
}
#endif
