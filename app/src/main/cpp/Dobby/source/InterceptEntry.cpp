#include "InterceptEntry.h"
#include "Interceptor.h"

InterceptEntry::InterceptEntry(InterceptEntryType type, addr_t address) {
  this->type = type;

#if defined(TARGET_ARCH_ARM)
  if (address % 2) {
    address -= 1;
    this->thumb_mode = true;
  }
#endif

  this->patched_addr = address;
  this->id = Interceptor::SharedInstance()->count();
}