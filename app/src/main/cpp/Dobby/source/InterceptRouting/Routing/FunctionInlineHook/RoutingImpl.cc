#include "dobby_internal.h"
#include "InterceptRouting/Routing/FunctionInlineHook/FunctionInlineHookRouting.h"

void FunctionInlineHookRouting::BuildRouting() {
  SetTrampolineTarget((addr_t)replace_func);

  // generate trampoline buffer, run before GenerateRelocatedCode
  addr_t from = entry_->patched_addr;
#if defined(TARGET_ARCH_ARM)
  if (entry_->thumb_mode)
    from += 1;
#endif
  addr_t to = GetTrampolineTarget();
  GenerateTrampolineBuffer(from, to);
}

void FunctionInlineHookRouting::DispatchRouting() {
  BuildRouting();

  // generate relocated code which size == trampoline size
  GenerateRelocatedCode();
}
