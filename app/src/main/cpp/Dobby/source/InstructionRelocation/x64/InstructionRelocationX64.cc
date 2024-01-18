#include "platform_macro.h"

#if defined(TARGET_ARCH_X64)

#include "dobby_internal.h"

#include "InstructionRelocation/x64/InstructionRelocationX64.h"
#include "InstructionRelocation/x86/x86_insn_decode/x86_insn_decode.h"

#include "core/arch/x64/registers-x64.h"
#include "core/assembler/assembler-x64.h"
#include "core/codegen/codegen-x64.h"

using namespace zz::x64;

int GenRelocateCodeFixed(void *buffer, CodeMemBlock *origin, CodeMemBlock *relocated, bool branch) {
  TurboAssembler turbo_assembler_(0);
  // Set fixed executable code chunk address
  turbo_assembler_.SetRealizedAddress((void *)relocated->addr);
#define _ turbo_assembler_.
#define __ turbo_assembler_.GetCodeBuffer()->

  auto curr_orig_ip = (addr64_t)origin->addr;
  auto curr_relo_ip = (addr64_t)relocated->addr;

  uint8_t *buffer_cursor = (uint8_t *)buffer;

  int predefined_relocate_size = origin->size;

  while ((buffer_cursor < ((uint8_t *)buffer + predefined_relocate_size))) {
    x86_insn_decode_t insn = {0};
    memset(&insn, 0, sizeof(insn));
    GenRelocateSingleX86Insn(curr_orig_ip, curr_relo_ip, buffer_cursor, turbo_assembler_.GetCodeBuffer(), insn, 64);

    // go next
    curr_orig_ip += insn.length;
    buffer_cursor += insn.length;
    curr_relo_ip = (addr64_t)relocated->addr + turbo_assembler_.ip_offset();
  }

  // jmp to the origin rest instructions
  if (branch) {
    CodeGen codegen(&turbo_assembler_);
    // TODO: 6 == jmp [RIP + disp32] instruction size
    addr64_t stub_addr = curr_relo_ip + 6;
    codegen.JmpNearIndirect(stub_addr);
    turbo_assembler_.GetCodeBuffer()->Emit64(curr_orig_ip);
  }

  // update origin
  int new_origin_len = curr_orig_ip - (addr_t)origin->addr;
  origin->reset(origin->addr, new_origin_len);

  int relo_len = turbo_assembler_.GetCodeBuffer()->GetBufferSize();
  if (relo_len > relocated->size) {
    DLOG(0, "pre-alloc code chunk not enough");
    return RT_FAILED;
  }

  // generate executable code
  {
    auto code = AssemblyCodeBuilder::FinalizeFromTurboAssembler(&turbo_assembler_);
    relocated->reset(code->addr, code->size);
    delete code;
  }

  return RT_SUCCESS;
}

void GenRelocateCodeAndBranch(void *buffer, CodeMemBlock *origin, CodeMemBlock *relocated) {
  GenRelocateCode(buffer, origin, relocated, true);
}

void GenRelocateCode(void *buffer, CodeMemBlock *origin, CodeMemBlock *relocated, bool branch) {
  GenRelocateCodeX86Shared(buffer, origin, relocated, branch);
}

#endif
