#include "InstructionRelocation/InstructionRelocation.h"
#include "InstructionRelocation/arm64/InstructionRelocationARM64.h"

#include <capstone.h>
#include <unicorn/unicorn.h>

#include <iostream>

class CapstoneDisassembler {
public:
  void disassemble(uintptr_t addr, char *buffer, size_t buffer_szie);

  static CapstoneDisassembler *Get(const std::string &arch);

private:
  CapstoneDisassembler(const std::string &arch, csh csh_);

  ~CapstoneDisassembler();

private:
  csh csh_;

  static CapstoneDisassembler *instance_;
};

CapstoneDisassembler *CapstoneDisassembler::instance_ = nullptr;

CapstoneDisassembler *CapstoneDisassembler::Get(const std::string &arch) {
  if (instance_ == nullptr) {
    cs_err err = CS_ERR_OK;
    csh csh_;
    if (arch == "arm") {
      err = cs_open(CS_ARCH_ARM, CS_MODE_ARM, &csh_);
    } else if (arch == "arm64") {
      err = cs_open(CS_ARCH_ARM64, CS_MODE_ARM, &csh_);
    } else if (arch == "x86_64") {
      err = cs_open(CS_ARCH_X86, CS_MODE_64, &csh_);
    } else if (arch == "x86") {
      err = cs_open(CS_ARCH_X86, CS_MODE_32, &csh_);
    }
    instance_ = new CapstoneDisassembler(arch, csh_);
  }
  return instance_;
}

CapstoneDisassembler::CapstoneDisassembler(const std::string &arch, csh csh_) : csh_(csh_) {
}

CapstoneDisassembler::~CapstoneDisassembler() {
  cs_close((csh *)&csh_);
}

void CapstoneDisassembler::disassemble(uintptr_t addr, char *buffer, size_t buffer_size) {
  cs_insn *insns;

  size_t count = cs_disasm(csh_, (uint8_t *)buffer, buffer_size, addr, 0, &insns);
  for (size_t i = 0; i < count; ++i) {
    auto &insn = insns[i];
    printf("%s %p: %s %s\n", "-", insn.address, insn.mnemonic, insn.op_str);
  }
  cs_free(insns, count);
}

class UniconEmulator {
public:
  UniconEmulator(const std::string &arch);

  void mapMemory(uintptr_t addr, char *buffer, size_t buffer_size);

  void *readRegister(int regId);

  void writeRegister(int regId, void *value);

  void start(uintptr_t addr, uintptr_t end);

  void stop() {
    uc_emu_stop(uc_);
  }

  void emulate(uintptr_t addr, char *buffer, size_t buffer_size);

  void setUnmappedAddr(uintptr_t addr) {
    unmapped_addr_ = addr;
  }

  intptr_t getFaultAddr() {
    return unmapped_addr_;
  }

  void reset();

private:
private:
  uc_err err_;
  uc_engine *uc_;
  uintptr_t unmapped_addr_;
};

static void hook_trace_insn(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
  auto emu = (UniconEmulator *)user_data;
  uc_err err;
  char insn_bytes[16];
  err = uc_mem_read(uc, address, insn_bytes, size);
  assert(err == UC_ERR_OK);
  CapstoneDisassembler::Get("arm64")->disassemble(address, (char *)insn_bytes, size);
}

static void hook_unmapped(uc_engine *uc, uc_mem_type type, uint64_t address, int size, int64_t value, void *user_data) {
  printf(">>> Unmapped memory access at %p, data size = %p, data value = %p\n", address, size, value);
  auto emu = (UniconEmulator *)user_data;
  emu->setUnmappedAddr(address);
  emu->stop();
}

void dump_regions(uc_engine *uc) {
  uc_mem_region *regions;
  uint32_t region_count;
  uc_mem_regions(uc, &regions, &region_count);
  for (int i = 0; i < region_count; ++i) {
    auto &region = regions[i];
    printf("region: %p - %p\n", region.begin, region.end);
  }
}

UniconEmulator::UniconEmulator(const std::string &arch) {
  uc_err err = UC_ERR_OK;
  if (arch == "arm") {
    err = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &uc_);
  } else if (arch == "arm64") {
    err = uc_open(UC_ARCH_ARM64, UC_MODE_ARM, &uc_);
  } else if (arch == "x86_64") {
    err = uc_open(UC_ARCH_X86, UC_MODE_64, &uc_);
  } else if (arch == "x86") {
    err = uc_open(UC_ARCH_X86, UC_MODE_32, &uc_);
  }
  assert(err == UC_ERR_OK);

  uc_hook hook_trace_insn_handle;
  uc_hook_add(uc_, &hook_trace_insn_handle, UC_HOOK_CODE, (void *)hook_trace_insn, this, 1, 0);

  uc_hook hook_unmapped_handle;
  uc_hook_add(uc_, &hook_unmapped_handle, UC_HOOK_MEM_UNMAPPED, (void *)hook_unmapped, this, 1, 0);
}

void UniconEmulator::mapMemory(uintptr_t addr, char *buffer, size_t buffer_size) {
  uc_err err = UC_ERR_OK;
  uintptr_t map_addr = ALIGN_FLOOR(addr, 0x1000);
  size_t map_size = ALIGN_CEIL(buffer_size, 0x1000);
  err = uc_mem_map(uc_, map_addr, map_size, UC_PROT_ALL);
  assert(err == UC_ERR_OK);
  err = uc_mem_write(uc_, addr, buffer, buffer_size);
  assert(err == UC_ERR_OK);
}

void *UniconEmulator::readRegister(int regId) {
  void *value = nullptr;
  uc_reg_read(uc_, regId, &value);
  return value;
}

void UniconEmulator::writeRegister(int regNdx, void *value) {
  uc_reg_write(uc_, regNdx, (void *)&value);
}

void UniconEmulator::start(uintptr_t addr, uintptr_t end) {
  uc_err err;
  err = uc_emu_start(uc_, addr, end, 0, 0);
  if (err == UC_ERR_FETCH_UNMAPPED || err ==  UC_ERR_READ_UNMAPPED || err == UC_ERR_WRITE_UNMAPPED)
    err = UC_ERR_OK;
  assert(err == UC_ERR_OK);
}

void UniconEmulator::emulate(uintptr_t addr, char *buffer, size_t buffer_size) {
  uc_err err;
  mapMemory(addr, buffer, buffer_size);
  writeRegister(UC_ARM_REG_PC, (void *)addr);
  start(addr, addr + buffer_size);
}

void check_insn_relo(char *buffer, size_t buffer_size, bool check_fault_addr, int check_reg_id,
                     void (^callback)(UniconEmulator *orig, UniconEmulator *relo)) {
  auto *orig_ue = new UniconEmulator("arm64");
  auto *relo_ue = new UniconEmulator("arm64");

  addr_t orig_addr = 0x100004000;
  addr_t relocate_addr = 0x200004000;

  //  auto dism = CapstoneDisassembler::Get("arm64");
  //  dism->disassemble((uintptr_t)orig_addr, buffer, buffer_size);
  //  printf("\n");

  auto origin = new CodeMemBlock(orig_addr, buffer_size);
  auto relocated = new CodeMemBlock();

  GenRelocateCode(buffer, origin, relocated, false);

  orig_ue->emulate(orig_addr, buffer, buffer_size);
  relo_ue->emulate(relocate_addr, (char *)relocated->addr, relocated->size);

  //  dism->disassemble((uintptr_t)relocate_addr, (char *)relocated->addr, relocated->size);
  //  printf("\n");

  if (check_fault_addr) {
    assert(orig_ue->getFaultAddr() == relo_ue->getFaultAddr());
  } else if (check_reg_id != -1) {
    assert(orig_ue->readRegister(check_reg_id) == relo_ue->readRegister(check_reg_id));
  } else if (callback) {
    callback(orig_ue, relo_ue);
  }

  delete orig_ue;
  delete relo_ue;
}

int main() {
  // b #-0x4000
  check_insn_relo("\x00\xf0\xff\x17", 4, true, -1, nullptr);
  // b #0x4000
  check_insn_relo("\x00\x10\x00\x14", 4, true, -1, nullptr);

  // bl #-0x4000
  check_insn_relo("\x00\xf0\xff\x97", 4, true, -1, nullptr);
  // bl #0x4000
  check_insn_relo("\x00\x10\x00\x94", 4, true, -1, nullptr);

  // mov x0, #0
  // cbz x0, #-0x4000
  check_insn_relo("\x00\x00\x80\xd2\x00\x00\xfe\xb4", 8, true, -1, nullptr);
  // mov x0, #0
  // cbz x0, #0x4000
  check_insn_relo("\x00\x00\x80\xd2\x00\x00\x02\xb4", 8, true, -1, nullptr);

  // ldr x0, #-0x4000
  check_insn_relo("\x00\x00\xfe\x58", 4, true, -1, nullptr);
  // ldr x0, #0x4000
  check_insn_relo("\x00\x00\x02\x58", 4, true, -1, nullptr);

  // adr x0, #-0x4000
  check_insn_relo("\x00\x00\xfe\x10", 4, false, UC_ARM64_REG_X0, nullptr);
  // adr x0, #0x4000
  check_insn_relo("\x00\x00\x02\x10", 4, false, UC_ARM64_REG_X0, nullptr);

  // adrp x0, #-0x4000
  check_insn_relo("\xe0\xff\xff\x90", 4, false, UC_ARM64_REG_X0, nullptr);
  // adrp x0, #0x4000
  check_insn_relo("\x20\x00\x00\x90", 4, false, UC_ARM64_REG_X0, nullptr);

  // mov x0, #0
  // cmp x0, #0
  // b.eq #-0x4000
  check_insn_relo("\x00\x00\x80\xd2\x1f\x00\x00\xf1\x00\x00\xfe\x54", 12, true, -1, nullptr);
  // mov x0, #0
  // cmp x0, #0
  // b.eq #0x4000
  check_insn_relo("\x00\x00\x80\xd2\x1f\x00\x00\xf1\x00\x00\x02\x54", 12, true, -1, nullptr);

  // mov x0, #0xb
  // tbz w0, 2, #-0x4000
  check_insn_relo("\x60\x01\x80\xd2\x00\x00\x16\x36", 8, true, -1, nullptr);
  // mov x0, #0xb

  // mov x0, #0xb
  // tbz w0, 2, #0x4000
  check_insn_relo("\x60\x01\x80\xd2\x00\x00\x12\x36", 8, true, -1, nullptr);

  return 0;
}
