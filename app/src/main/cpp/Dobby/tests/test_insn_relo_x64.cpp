#include "InstructionRelocation/InstructionRelocation.h"

#include "UniconEmulator.h"

int main() {
  log_set_level(0);
  set_global_arch("x86_64");


  // cmp eax, eax
  // jz -0x20
  check_insn_relo("\x39\xc0\x74\xdc", 4, false, UC_X86_REG_IP, nullptr);
  // cmp eax, eax
  // jz 0x20
  check_insn_relo("\x39\xc0\x74\x1c", 4, false, UC_X86_REG_IP, nullptr);

  // jmp -0x20
  check_insn_relo("\xeb\xde", 2, false, UC_X86_REG_IP, nullptr);
  // jmp 0x20
  check_insn_relo("\xeb\x1e", 2, false, UC_X86_REG_IP, nullptr);


  // jmp -0x4000
  check_insn_relo("\xe9\xfb\xbf\xff\xff", 4, false, UC_X86_REG_IP, nullptr);
  // jmp 0x4000
  check_insn_relo("\xe9\xfb\x3f\x00\x00", 4, false, UC_X86_REG_IP, nullptr);

  // lea rax, [rip]
  check_insn_relo("\x48\x8d\x05\x00\x00\x00\x00", 7, false, UC_X86_REG_RAX, nullptr);

  // lea rax, [rip + 0x4000]
  check_insn_relo("\x48\x8d\x05\x00\x40\x00\x00", 7, false, UC_X86_REG_RAX, nullptr);

  // mov rax, [rip + 0x4000]
  check_insn_relo("\x48\x8b\x05\x00\x40\x00\x00", 7, true, -1, nullptr);

  return 0;
}
