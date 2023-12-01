#include "UnifiedInterface/platform.h"

#include <sys/mman.h>
#include <mach/mach_vm.h>
#include <vm/vm_map.h>
#include <kern/debug.h>

// ================================================================
// base :: OSMemory

static int GetProtectionFromMemoryPermission(MemoryPermission access) {
  switch (access) {
  case MemoryPermission::kNoAccess:
    return PROT_NONE;
  case MemoryPermission::kRead:
    return PROT_READ;
  case MemoryPermission::kReadWrite:
    return PROT_READ | PROT_WRITE;
  case MemoryPermission::kReadWriteExecute:
    return PROT_READ | PROT_WRITE | PROT_EXEC;
  case MemoryPermission::kReadExecute:
    return PROT_READ | PROT_EXEC;
  }
  UNREACHABLE();
}

int OSMemory::PageSize() {
  return static_cast<int>(0x4000);
}

void *OSMemory::Allocate(size_t size, MemoryPermission access) {
  return OSMemory::Allocate(size, access, nullptr);
}

extern "C" void *kernel_executable_memory_placeholder;
void *OSMemory::Allocate(size_t size, MemoryPermission access, void *fixed_address) {
  int prot = GetProtectionFromMemoryPermission(access);

  void *addr = nullptr;
  int flags = VM_FLAGS_ANYWHERE;
  if (fixed_address != nullptr) {
    flags = VM_FLAGS_FIXED;
    addr = fixed_address;
  }

  // fixme: wire at pmap
  if (prot & PROT_EXEC || prot == PROT_NONE) {
    addr = &kernel_executable_memory_placeholder;
  } else {
    kern_return_t ret = mach_vm_allocate(kernel_map, (mach_vm_address_t *)&addr, size, flags);
    if (ret != KERN_SUCCESS) {
      panic("mach_vm_allocate");
      return nullptr;
    }
    ret = vm_map_wire(kernel_map, (mach_vm_address_t)addr, (mach_vm_address_t)addr + size, PROT_NONE, false);
    if (ret != KERN_SUCCESS) {
      panic("vm_map_wire");
      return nullptr;
    }

    // make fault before at rw prot
    bzero(addr, size);
    { memcpy(addr, "AAAAAAAA", 8); }

    if (access == kNoAccess) {
      access = kReadExecute;
    }
    if (!OSMemory::SetPermission((void *)addr, size, access)) {
      OSMemory::Free(addr, size);
      return nullptr;
    }

    {
      if (memcmp(addr, "AAAAAAAA", 8) != 0) {
        return nullptr;
      }
    }
  }

  return addr;
}

bool OSMemory::Free(void *address, size_t size) {
  DCHECK_EQ(0, reinterpret_cast<uintptr_t>(address) % PageSize());
  DCHECK_EQ(0, size % PageSize());

  auto ret = mach_vm_deallocate(kernel_map, (mach_vm_address_t)address, size);
  return ret == KERN_SUCCESS;
}

bool OSMemory::Release(void *address, size_t size) {
  DCHECK_EQ(0, reinterpret_cast<uintptr_t>(address) % PageSize());
  DCHECK_EQ(0, size % PageSize());

  auto ret = mach_vm_deallocate(kernel_map, (mach_vm_address_t)address, size);
  return ret == KERN_SUCCESS;
}

bool OSMemory::SetPermission(void *address, size_t size, MemoryPermission access) {
  DCHECK_EQ(0, reinterpret_cast<uintptr_t>(address) % PageSize());
  DCHECK_EQ(0, size % PageSize());

  int prot = GetProtectionFromMemoryPermission(access);
  auto ret = mach_vm_protect(kernel_map, (mach_vm_address_t)address, size, false, prot);
  return ret == KERN_SUCCESS;
}
