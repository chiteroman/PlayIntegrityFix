#include "MemoryAllocator/AssemblyCodeBuilder.h"

#include "dobby/dobby_internal.h"
#include "PlatformUnifiedInterface/ExecMemory/CodePatchTool.h"

AssemblyCode *AssemblyCodeBuilder::FinalizeFromTurboAssembler(AssemblerBase *assembler) {
  auto buffer = (CodeBufferBase *)assembler->GetCodeBuffer();
  auto realized_addr = (addr_t)assembler->GetRealizedAddress();
#if defined(TEST_WITH_UNICORN)
  // impl: unicorn emulator map memory
  realized_addr = 0;
#endif
  if (!realized_addr) {
    size_t buffer_size = 0;
    buffer_size = buffer->GetBufferSize();
#if TARGET_ARCH_ARM
    // extra bytes for align needed
    buffer_size += 4;
#endif

    auto block = MemoryAllocator::SharedAllocator()->allocateExecBlock(buffer_size);
    if (block == nullptr)
      return nullptr;

    realized_addr = block->addr;
    assembler->SetRealizedAddress((void *)realized_addr);
  }

  // Realize the buffer code to the executable memory address, remove the external label, etc
  DobbyCodePatch((void *)realized_addr, buffer->GetBuffer(), buffer->GetBufferSize());

  auto block = new AssemblyCode(realized_addr, buffer->GetBufferSize());
  return block;
}