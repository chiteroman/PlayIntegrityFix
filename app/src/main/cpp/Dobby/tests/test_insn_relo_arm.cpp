#include "InstructionRelocation/InstructionRelocation.h"

#include "UniconEmulator.h"

void check_insn_relo_arm(char *buffer, size_t buffer_size, bool check_fault_addr, int check_reg_id,
                         void (^callback)(UniconEmulator *orig, UniconEmulator *relo)) {
  __attribute__((aligned(4))) char code[64] = {0};
  memcpy(code, buffer, buffer_size);
  check_insn_relo(code, buffer_size, check_fault_addr, check_reg_id, callback);
}

void check_insn_relo_thumb(char *buffer, size_t buffer_size, bool check_fault_addr, int check_reg_id,
                           void (^callback)(UniconEmulator *orig, UniconEmulator *relo), uintptr_t relo_stop_size = 0) {
  __attribute__((aligned(4))) char code[64] = {0};
  memcpy(code, buffer, buffer_size);
  check_insn_relo(code, buffer_size, check_fault_addr, check_reg_id, callback, relo_stop_size);
}

int main() {
  log_set_level(0);
  set_global_arch("arm");

  // ldr r0, [pc, #-0x20]
  __attribute__((aligned(4))) char *code_0 = "\x20\x00\x1f\xe5";
  check_insn_relo(code_0, 4, true, -1, nullptr);

  // ldr r0, [pc, #0x20]
  __attribute__((aligned(4))) char *code_1 = "\x20\x00\x9f\xe5";
  check_insn_relo(code_1, 4, false, -1, ^(UniconEmulator *orig, UniconEmulator *relo) {
    assert(relo->getFaultAddr() == 0x10014028);
  });

  // add r0, pc, #-0x4000
  check_insn_relo_arm("\x01\x09\x4f\xe2", 4, false, UC_ARM_REG_R0, nullptr);
  // add r0, pc, #0x4000
  check_insn_relo_arm("\x01\x09\x8f\xe2", 4, false, UC_ARM_REG_R0, nullptr);

  // b #-0x4000
  check_insn_relo_arm("\xfe\xef\xff\xea", 4, true, -1, nullptr);
  // b #0x4000
  check_insn_relo_arm("\xfe\x0f\x00\xea", 4, true, -1, nullptr);

  // bl #-0x4000
  check_insn_relo_arm("\xfe\xef\xff\xeb", 4, true, -1, nullptr);
  // blx #0x4000
  check_insn_relo_arm("\xfe\x0f\x00\xfa", 4, true, -1, nullptr);

  set_global_arch("thumb");

  // cmp r0, pc
  check_insn_relo_thumb("\x78\x45", 2, false, -1, ^(UniconEmulator *orig, UniconEmulator *relo) {
    assert(relo->readRegister(UC_ARM_REG_R12) == (void *)0x10014004);
  });

  // adr r0, #0x20
  check_insn_relo_thumb("\x08\xa0", 2, false, UC_ARM_REG_R0, nullptr, 8);

  // bx pc
  check_insn_relo_thumb("\x78\x47", 2, false, UC_ARM_REG_PC, nullptr);
  // blx pc
  check_insn_relo_thumb("\xf8\x47", 2, false, UC_ARM_REG_PC, nullptr);

  // ldr r0, [pc, #8]
  check_insn_relo_thumb("\x02\x48", 2, false, -1, ^(UniconEmulator *orig, UniconEmulator *relo) {
    assert(relo->getFaultAddr() == 0x1001400c);
  });

  // b #-8
  check_insn_relo_thumb("\xfa\xe7", 2, true, -1, nullptr);
  // b #8
  check_insn_relo_thumb("\x02\xe0", 2, false, -1, ^(UniconEmulator *orig, UniconEmulator *relo) {
    assert(relo->getFaultAddr() == 0x10014008);
  });

  // mov r0, 0
  // cbz r0, #8
  check_insn_relo_thumb("\x4f\xf0\x00\x00"
                        "\x10\xb1",
                        6, false, -1, ^(UniconEmulator *orig, UniconEmulator *relo) {
                          assert(relo->getFaultAddr() == 0x1001400c);
                        });

  set_global_arch("thumb");

  // cmp r0, r0
  // beq.w #-0x4000
  check_insn_relo_thumb("\x80\x42"
                        "\x3c\xf4\x00\xa8",
                        6, true, -1, nullptr);
  // cmp r0, r0
  // beq.w #0x4000
  check_insn_relo_thumb("\x80\x42"
                        "\x04\xf0\x00\x80",
                        6, true, -1, nullptr);

  // bl #-0x4000
  check_insn_relo_thumb("\xfb\xf7\xfe\xff", 4, true, -1, nullptr);
  // blx #0x4000
  check_insn_relo_thumb("\x03\xf0\xfe\xef", 4, true, -1, nullptr);

  // adr r0, #-0x512
  check_insn_relo_thumb("\xaf\xf2\x12\x50", 4, false, UC_ARM_REG_R0, nullptr);
  // adr r0, #0x512
  check_insn_relo_thumb("\x0f\xf2\x12\x50", 4, false, UC_ARM_REG_R0, nullptr);

  // ldr r0, [pc, #-0x512]
  check_insn_relo_thumb("\x5f\xf8\x12\x05", 4, true, -1, nullptr, 0xc);
  // ldr r0, [pc, #0x512]
  check_insn_relo_thumb(
      "\xdf\xf8\x12\x05", 4, false, -1,
      ^(UniconEmulator *orig, UniconEmulator *relo) {
        assert(relo->getFaultAddr() == 0x10014000 + 0x512 + 4);
      },
      0xc);
  return 0;
}
