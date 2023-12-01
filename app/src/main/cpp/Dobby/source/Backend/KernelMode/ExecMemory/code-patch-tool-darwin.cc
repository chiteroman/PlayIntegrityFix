#include "dobby/dobby_internal.h"

#include "PlatformUnifiedInterface/ExecMemory/ClearCacheTool.h"

#include <mach/mach_types.h>
#include <vm/vm_kern.h>
#include <mach/mach_vm.h>
#include <ptrauth.h>

#undef max
#undef min
#include <libkern/libkern.h>

#define DobbySymbolResolverAuth(o_var, name)                                                                           \
  do {                                                                                                                 \
    static void *func_ptr = nullptr;                                                                                   \
    if (func_ptr == nullptr) {                                                                                         \
      func_ptr = DobbySymbolResolver(nullptr, name);                                                                   \
      if (func_ptr) {                                                                                                  \
        func_ptr = ptrauth_strip((void *)func_ptr, ptrauth_key_asia);                                                  \
        func_ptr = ptrauth_sign_unauthenticated(func_ptr, ptrauth_key_asia, 0);                                        \
      }                                                                                                                \
    }                                                                                                                  \
    o_var = (typeof(o_var))func_ptr;                                                                                   \
  } while (0);

#define KERN_RETURN_ERROR(kr, failure)                                                                                 \
  do {                                                                                                                 \
    if (kr != KERN_SUCCESS) {                                                                                          \
      ERROR_LOG("mach error: %d", kr);                                                                                 \
      return failure;                                                                                                  \
    }                                                                                                                  \
  } while (0);

PUBLIC int DobbyCodePatch(void *address, uint8_t *buffer, uint32_t buffer_size) {
  if (address == nullptr || buffer == nullptr || buffer_size == 0) {
    ERROR_LOG("invalid argument");
    return kMemoryOperationError;
  }

  kern_return_t kr;

  {
    paddr_t dst_paddr = pmap_kit_kvtophys(kernel_pmap, (vaddr_t)address);
    paddr_t src_paddr = pmap_kit_kvtophys(kernel_pmap, (vaddr_t)buffer);
    pmap_kit_bcopy_phys((addr_t)buffer, dst_paddr, buffer_size, cppvPsnk);
    DEBUG_LOG("bcopy_phys: src: %p, dst: %p", src_paddr, dst_paddr);

    pmap_kit_kva_to_pte(kernel_pmap, (vaddr_t)address);
    pmap_kit_set_perm(kernel_pmap, (vaddr_t)address, (vaddr_t)address + PAGE_SIZE, VM_PROT_READ | VM_PROT_EXECUTE);

    if (memcmp(address, buffer, buffer_size))
      return kMemoryOperationError;
  }

  return 0;
}
