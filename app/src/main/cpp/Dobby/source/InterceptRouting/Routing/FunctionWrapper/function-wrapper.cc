#include "dobby_internal.h"

#include "TrampolineBridge/ClosureTrampolineBridge/ClosureTrampoline.h"

#include "intercept_routing_handler.h"

#include "function-wrapper.h"

void FunctionWrapperRouting::DispatchRouting() {
  Prepare();
  BuildPreCallRouting();
  BuildPostCallRouting();
}

// Add pre_call(prologue) handler before running the origin function,
void FunctionWrapperRouting::BuildPreCallRouting() {
  // create closure trampoline jump to prologue_routing_dispath with the `entry_` data
  ClosureTrampolineEntry *cte = ClosureTrampoline::CreateClosureTrampoline(entry_, (void *)prologue_routing_dispatch);
  this->prologue_dispatch_bridge = cte->address;

  DLOG(0, "Create pre call closure trampoline to 'prologue_routing_dispatch' at %p", cte->address);
}

// Add post_call(epilogue) handler before `Return` of the origin function, as implementation is replace the origin
// `Return Address` of the function.
void FunctionWrapperRouting::BuildPostCallRouting() {
  // create closure trampoline jump to prologue_routing_dispath with the `entry_` data
  ClosureTrampolineEntry *closure_trampoline_entry;
  // format trampoline
  closure_trampoline_entry = ClosureTrampoline::CreateClosureTrampoline(entry_, (void *)epilogue_routing_dispatch);
  DLOG(0, "Create post call closure trampoline to 'prologue_routing_dispatch' at %p",
       closure_trampoline_entry->address);

  this->SetTrampolineTarget(closure_trampoline_entry->address);
  this->epilogue_dispatch_bridge = closure_trampoline_entry->address;

  GenerateTrampolineBuffer(entry_->target_address, GetTrampolineTarget());
}