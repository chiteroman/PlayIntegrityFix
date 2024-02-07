#include "UniconEmulator.h"
#include "PlatformUnifiedInterface/MemoryAllocator.h"
#include "InstructionRelocation/InstructionRelocation.h"

// align
#ifndef ALIGN
#define ALIGN ALIGN_FLOOR
#endif
#define ALIGN_FLOOR(address, range) ((uintptr_t)address & ~((uintptr_t)range - 1))
#define ALIGN_CEIL(address, range) (((uintptr_t)address + (uintptr_t)range - 1) & ~((uintptr_t)range - 1))

std::string g_arch = "";

void set_global_arch(std::string arch) {
  g_arch = arch;
}

std::unordered_map<std::string, CapstoneDisassembler *> CapstoneDisassembler::instances_;

CapstoneDisassembler *CapstoneDisassembler::Get(const std::string &arch) {
  if (instances_.count(arch) == 0) {
    cs_err err = CS_ERR_OK;
    csh csh_;
    if (arch == "arm") {
      err = cs_open(CS_ARCH_ARM, CS_MODE_ARM, &csh_);
    } else if (arch == "thumb") {
      err = cs_open(CS_ARCH_ARM, CS_MODE_THUMB, &csh_);
    } else if (arch == "arm64") {
      err = cs_open(CS_ARCH_ARM64, CS_MODE_ARM, &csh_);
    } else if (arch == "x86_64") {
      err = cs_open(CS_ARCH_X86, CS_MODE_64, &csh_);
    } else if (arch == "x86") {
      err = cs_open(CS_ARCH_X86, CS_MODE_32, &csh_);
    }

    auto instance = new CapstoneDisassembler(arch, csh_);
    instances_[arch] = instance;
  }
  return instances_[arch];
}

CapstoneDisassembler::CapstoneDisassembler(const std::string &arch, csh csh_) : arch_(arch), csh_(csh_) {
}

CapstoneDisassembler::~CapstoneDisassembler() {
  cs_close((csh *)&csh_);
}

void CapstoneDisassembler::disassemble(uintptr_t addr, char *buffer, size_t buffer_size) {
  cs_insn *insns;

  size_t count = cs_disasm(csh_, (uint8_t *)buffer, buffer_size, addr, 0, &insns);
  for (size_t i = 0; i < count; ++i) {
    auto &insn = insns[i];
    if (arch_ == "thumb") {
      printf("%s %p: %s %s // thumb-%d\n", "-", insn.address, insn.mnemonic, insn.op_str, insn.size / 2);
    } else {
      printf("%s %p: %s %s\n", "-", insn.address, insn.mnemonic, insn.op_str);
    }
  }
  cs_free(insns, count);
}

static void hook_trace_insn(uc_engine *uc, uint64_t address, uint32_t size, void *user_data) {
  auto emu = (UniconEmulator *)user_data;

  uc_err err;
  char insn_bytes[16];
  err = uc_mem_read(uc, address, insn_bytes, size);
  assert(err == UC_ERR_OK);

  if (address >= emu->end_) {
    emu->stop();
    return;
  }

  if ((emu->arch_ == "arm" || emu->arch_ == "thumb") && emu->isThumb()) {
    CapstoneDisassembler::Get("thumb")->disassemble(address, (char *)insn_bytes, size);
  } else {
    CapstoneDisassembler::Get(g_arch)->disassemble(address, (char *)insn_bytes, size);
  }
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
  if (arch == "arm" || arch == "thumb") {
    err = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &uc_);
  } else if (arch == "arm64") {
    err = uc_open(UC_ARCH_ARM64, UC_MODE_ARM, &uc_);
  } else if (arch == "x86_64") {
    err = uc_open(UC_ARCH_X86, UC_MODE_64, &uc_);
  } else if (arch == "x86") {
    err = uc_open(UC_ARCH_X86, UC_MODE_32, &uc_);
  }
  assert(err == UC_ERR_OK);

  arch_ = arch;

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
  if (g_arch == "thumb") {
    addr |= 1;
  }
  err = uc_emu_start(uc_, addr, end, 0, 0);
  if (err == UC_ERR_FETCH_UNMAPPED || err == UC_ERR_READ_UNMAPPED || err == UC_ERR_WRITE_UNMAPPED)
    err = UC_ERR_OK;
  assert(err == UC_ERR_OK);
}

void UniconEmulator::emulate(uintptr_t addr, uintptr_t end, char *buffer, size_t buffer_size) {
  uc_err err;
  mapMemory(addr, buffer, buffer_size);
  writeRegister(UC_ARM_REG_PC, (void *)addr);

  if (end == 0)
    end = addr + buffer_size;

  start_ = addr;
  end_ = end;

  start(addr, end);
}

void check_insn_relo(char *buffer, size_t buffer_size, bool check_fault_addr, int check_reg_id,
                     void (^callback)(UniconEmulator *orig, UniconEmulator *relo), uintptr_t relo_stop_size) {
  auto *orig_ue = new UniconEmulator(g_arch);
  auto *relo_ue = new UniconEmulator(g_arch);

  addr_t orig_addr = 0x100014000;
  addr_t relocate_addr = 0x100024000;

  if (g_arch == "arm" || g_arch == "thumb") {
    orig_addr = 0x10014000;
    relocate_addr = 0x10024000;
  }

  //  auto dism = CapstoneDisassembler::Get("arm64");
  //  dism->disassemble((uintptr_t)orig_addr, buffer, buffer_size);
  //  printf("\n");

  auto origin = new CodeMemBlock(orig_addr, buffer_size);
  auto relocated = new CodeMemBlock(relocate_addr, 0x1000);
  if (g_arch == "thumb") {
    origin->reset(origin->addr + 1, origin->size);
  }

  GenRelocateCode(buffer, origin, relocated, false);

  if (g_arch == "thumb") {
    orig_ue->writeRegister(UC_ARM_REG_CPSR, (void *)0x20);
    relo_ue->writeRegister(UC_ARM_REG_CPSR, (void *)0x20);
  }
  orig_ue->emulate(orig_addr, 0, buffer, buffer_size);
  if (g_arch == "thumb") {
    relocated->addr -= 1;
  }
  if (relo_stop_size == 0) {
    relo_stop_size = relocated->size;
  }
  relo_ue->emulate(relocate_addr, relocate_addr + relo_stop_size, (char *)relocated->addr, relocated->size);

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
