#include "dobby/dobby_internal.h"

#include "Interceptor.h"
#include "InterceptRouting/InterceptRouting.h"
#include "InterceptRouting/Routing/InstructionInstrument/InstructionInstrumentRouting.h"

PUBLIC int DobbyInstrument(void *address, dobby_instrument_callback_t pre_handler) {
  if (!address) {
    ERROR_LOG("address is 0x0.\n");
    return -1;
  }

#if defined(__APPLE__) && defined(__arm64__)
  address = pac_strip(address);
#endif

#if defined(ANDROID)
  void *page_align_address = (void *)ALIGN_FLOOR(address, OSMemory::PageSize());
  if (!OSMemory::SetPermission(page_align_address, OSMemory::PageSize(), kReadExecute)) {
    return -1;
  }
#endif

  DEBUG_LOG("\n\n----- [DobbyInstrument:%p] -----", address);

  auto entry = Interceptor::SharedInstance()->find((addr_t)address);
  if (entry) {
    ERROR_LOG("%s already been instrumented.", address);
    return -1;
  }

  entry = new InterceptEntry(kInstructionInstrument, (addr_t)address);

  auto routing = new InstructionInstrumentRouting(entry, pre_handler, nullptr);
  routing->Prepare();
  routing->DispatchRouting();
  routing->Commit();

  Interceptor::SharedInstance()->add(entry);

  return 0;
}
