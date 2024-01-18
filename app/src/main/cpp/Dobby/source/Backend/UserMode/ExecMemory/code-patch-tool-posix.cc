
#include "dobby_internal.h"
#include "core/arch/Cpu.h"

#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

#if !defined(__APPLE__)
PUBLIC MemoryOperationError DobbyCodePatch(void *address, uint8_t *buffer, uint32_t buffer_size) {
#if defined(__ANDROID__) || defined(__linux__)
  int page_size = (int)sysconf(_SC_PAGESIZE);
  uintptr_t patch_page = ALIGN_FLOOR(address, page_size);
  uintptr_t patch_end_page = ALIGN_FLOOR((uintptr_t)address + buffer_size, page_size);

  // change page permission as rwx
  mprotect((void *)patch_page, page_size, PROT_READ | PROT_WRITE | PROT_EXEC);
  if (patch_page != patch_end_page) {
    mprotect((void *)patch_end_page, page_size, PROT_READ | PROT_WRITE | PROT_EXEC);
  }

  // patch buffer
  memcpy(address, buffer, buffer_size);

  // restore page permission
  mprotect((void *)patch_page, page_size, PROT_READ | PROT_EXEC);
  if (patch_page != patch_end_page) {
    mprotect((void *)patch_end_page, page_size, PROT_READ | PROT_EXEC);
  }

  addr_t clear_start_ = (addr_t)address;
  ClearCache((void *)clear_start_, (void *)(clear_start_ + buffer_size));
#endif
  return kMemoryOperationSuccess;
}

#endif