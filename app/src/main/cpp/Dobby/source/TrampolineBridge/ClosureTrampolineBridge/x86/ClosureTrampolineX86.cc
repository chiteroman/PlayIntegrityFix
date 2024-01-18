#include "platform_macro.h"
#if defined(TARGET_ARCH_IA32)

#include "dobby_internal.h"

#include "core/assembler/assembler-ia32.h"

#include "TrampolineBridge/ClosureTrampolineBridge/ClosureTrampoline.h"

using namespace zz;
using namespace zz::x86;

ClosureTrampolineEntry *ClosureTrampoline::CreateClosureTrampoline(void *carry_data, void *carry_handler) {
  ClosureTrampolineEntry *tramp_entry = nullptr;
  tramp_entry = new ClosureTrampolineEntry;

  auto tramp_size = 32;
  auto tramp_mem = MemoryAllocator::SharedAllocator()->allocateExecMemory(tramp_size);
  if (tramp_mem == nullptr) {
    return nullptr;
  }

#define _ turbo_assembler_.
#define __ turbo_assembler_.GetCodeBuffer()->
  TurboAssembler turbo_assembler_(tramp_mem);

  int32_t offset = (int32_t)((uintptr_t)get_closure_bridge() - ((uintptr_t)tramp_mem + 18));

  _ sub(esp, Immediate(4, 32));
  _ mov(Address(esp, 4 * 0), Immediate((int32_t)(uintptr_t)tramp_entry, 32));
  _ jmp(Immediate(offset, 32));

  tramp_entry->address = tramp_mem;
  tramp_entry->size = tramp_size;
  tramp_entry->carry_data = carry_data;
  tramp_entry->carry_handler = carry_handler;

  auto closure_tramp_buffer = static_cast<CodeBufferBase *>(turbo_assembler_.GetCodeBuffer());
  DobbyCodePatch(tramp_mem, (uint8_t *)closure_tramp_buffer->GetBuffer(), closure_tramp_buffer->GetBufferSize());

  return tramp_entry;
}

#endif