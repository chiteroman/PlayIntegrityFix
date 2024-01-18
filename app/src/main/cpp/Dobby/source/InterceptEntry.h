#pragma once

#include <stdint.h>
#include "common_header.h"

typedef enum { kFunctionInlineHook, kInstructionInstrument } InterceptEntryType;

class InterceptRouting;

typedef struct InterceptEntry {
  uint32_t id;
  InterceptEntryType type;
  InterceptRouting *routing;

  union {
    addr_t addr;
    addr_t patched_addr;
  };
  uint32_t patched_size;

  addr_t relocated_addr;
  uint32_t relocated_size;

  uint8_t origin_insns[256];
  uint32_t origin_insn_size;

  bool thumb_mode;

  InterceptEntry(InterceptEntryType type, addr_t address);
} InterceptEntry;