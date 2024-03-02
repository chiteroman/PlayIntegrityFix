#include "dobby/dobby_internal.h"
#include "Interceptor.h"

__attribute__((constructor)) static void ctor() {
  DEBUG_LOG("================================");
  DEBUG_LOG("Dobby");
  DEBUG_LOG("dobby in debug log mode, disable with cmake flag \"-DDOBBY_DEBUG=OFF\"");
  DEBUG_LOG("================================");
}

PUBLIC const char *DobbyGetVersion() {
  return "";
}

PUBLIC int DobbyDestroy(void *address) {
#if defined(TARGET_ARCH_ARM)
  if ((addr_t)address % 2) {
    address = (void *)((addr_t)address - 1);
  }
#endif
  auto entry = Interceptor::SharedInstance()->find((addr_t)address);
  if (entry) {
    uint8_t *buffer = entry->origin_insns;
    uint32_t buffer_size = entry->origin_insn_size;
    DobbyCodePatch(address, buffer, buffer_size);
    Interceptor::SharedInstance()->remove((addr_t)address);
    return 0;
  }

  return -1;
}
