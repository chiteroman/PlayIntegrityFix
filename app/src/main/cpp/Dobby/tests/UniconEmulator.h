#pragma once

#include <capstone.h>
#include <unicorn/unicorn.h>

#include <iostream>
#include <unordered_map>

class CapstoneDisassembler {
public:
  void disassemble(uintptr_t addr, char *buffer, size_t buffer_size);

  static CapstoneDisassembler *Get(const std::string &arch);

private:
  CapstoneDisassembler(const std::string &arch, csh csh_);

  ~CapstoneDisassembler();

private:
  std::string arch_;
  csh csh_;

  static std::unordered_map<std::string, CapstoneDisassembler *> instances_;
};

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

  void emulate(uintptr_t addr, uintptr_t end, char *buffer, size_t buffer_size);

  void setUnmappedAddr(uintptr_t addr) {
    unmapped_addr_ = addr;
  }

  intptr_t getFaultAddr() {
    return unmapped_addr_;
  }

  bool isThumb() {
    void *reg_value = readRegister(UC_ARM_REG_CPSR);
    return (intptr_t)reg_value & 0x20;
  }

  void reset();

public:
  std::string arch_;
  uintptr_t start_, end_;

private:
  uc_err err_;
  uc_engine *uc_;
  uintptr_t unmapped_addr_;
};

void set_global_arch(std::string arch);

void check_insn_relo(char *buffer, size_t buffer_size, bool check_fault_addr, int check_reg_id,
                     void (^callback)(UniconEmulator *orig, UniconEmulator *relo), uintptr_t relo_stop_size = 0);