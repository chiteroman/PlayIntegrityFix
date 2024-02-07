#include "platform_detect_macro.h"

#if defined(TARGET_ARCH_IA32)

#include "dobby/dobby_internal.h"

#include "InstructionRelocation/x86/InstructionRelocationX86.h"
#include "InstructionRelocation/x86/x86_insn_decode/x86_insn_decode.h"

#include "core/arch/x86/registers-x86.h"
#include "core/assembler/assembler-ia32.h"
#include "core/codegen/codegen-ia32.h"

using namespace zz::x86;

int GenRelocateCodeFixed(void *buffer, CodeMemBlock *origin, CodeMemBlock *relocated, bool branch) {
  TurboAssembler turbo_assembler_(0);
  // Set fixed executable code chunk address
  turbo_assembler_.SetRealizedAddress((void *)relocated->addr);
#define _ turbo_assembler_.
#define __ turbo_assembler_.GetCodeBuffer()->

  auto curr_orig_ip = (addr32_t)origin->addr;
  auto curr_relo_ip = (addr32_t)relocated->addr;

  uint8_t *buffer_cursor = (uint8_t *)buffer;

  x86_options_t conf = {0};
  conf.mode = 32;

  int predefined_relocate_size = origin->size;

  while ((buffer_cursor < ((uint8_t *)buffer + predefined_relocate_size))) {
    x86_insn_decode_t insn = {0};
    memset(&insn, 0, sizeof(insn));
    GenRelocateSingleX86Insn(curr_orig_ip, curr_relo_ip, buffer_cursor, &turbo_assembler_,
                             turbo_assembler_.GetCodeBuffer(), insn, 64);

    // go next
    curr_orig_ip += insn.length;
    buffer_cursor += insn.length;
    curr_relo_ip = (addr32_t)relocated->addr + turbo_assembler_.ip_offset();
  }

  // jmp to the origin rest instructions
  if (branch) {
    CodeGen codegen(&turbo_assembler_);
    addr32_t stub_addr = curr_relo_ip + 6;
    codegen.JmpNear(curr_orig_ip);
  }

  // update origin
  int new_origin_len = curr_orig_ip - (addr_t)origin->addr;
  origin->reset(origin->addr, new_origin_len);

  int relo_len = turbo_assembler_.GetCodeBuffer()->GetBufferSize();
  if (relo_len > relocated->size) {
    DEBUG_LOG("pre-alloc code chunk not enough");
    return -1;
  }

  // generate executable code
  {
    auto code = AssemblyCodeBuilder::FinalizeFromTurboAssembler(&turbo_assembler_);
    relocated->reset(code->addr, code->size);
    delete code;
  }

  return 0;
}

void GenRelocateCodeAndBranch(void *buffer, CodeMemBlock *origin, CodeMemBlock *relocated) {
  GenRelocateCode(buffer, origin, relocated, true);
}

void GenRelocateCode(void *buffer, CodeMemBlock *origin, CodeMemBlock *relocated, bool branch) {
  GenRelocateCodeX86Shared(buffer, origin, relocated, branch);
}

#endif
