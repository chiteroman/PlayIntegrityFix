#include "dobby_internal.h"

#include "Interceptor.h"
#include "InterceptRouting/Routing/FunctionInlineHook/FunctionInlineHookRouting.h"

PUBLIC int DobbyHook(void *address, dobby_dummy_func_t replace_func, dobby_dummy_func_t *origin_func) {
  if (!address) {
    ERROR_LOG("function address is 0x0");
    return RS_FAILED;
  }

#if defined(__APPLE__) && defined(__arm64__)
#if __has_feature(ptrauth_calls)
  address = ptrauth_strip(address, ptrauth_key_asia);
  replace_func = ptrauth_strip(replace_func, ptrauth_key_asia);
#endif
#endif

#if defined(ANDROID)
  void *page_align_address = (void *)ALIGN_FLOOR(address, OSMemory::PageSize());
  if (!OSMemory::SetPermission(page_align_address, OSMemory::PageSize(), kReadExecute)) {
    return RS_FAILED;
  }
#endif

  DLOG(0, "----- [DobbyHook:%p] -----", address);

  // check if already register
  auto entry = Interceptor::SharedInstance()->find((addr_t)address);
  if (entry) {
    ERROR_LOG("%p already been hooked.", address);
    return RS_FAILED;
  }

  entry = new InterceptEntry(kFunctionInlineHook, (addr_t)address);

  auto *routing = new FunctionInlineHookRouting(entry, replace_func);
  routing->Prepare();
  routing->DispatchRouting();

  // set origin func entry with as relocated instructions
  if (origin_func) {
    *origin_func = (dobby_dummy_func_t)entry->relocated_addr;
  }

  routing->Commit();

  Interceptor::SharedInstance()->add(entry);

  return RS_SUCCESS;
}
