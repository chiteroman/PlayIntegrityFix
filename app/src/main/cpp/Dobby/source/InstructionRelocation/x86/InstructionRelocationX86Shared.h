#pragma once

#include "dobby/common.h"

#include "MemoryAllocator/AssemblyCodeBuilder.h"

#include "x86_insn_decode/x86_insn_decode.h"

#include "X86DecodeKit.h"

int GenRelocateCodeFixed(void *buffer, CodeMemBlock *origin, CodeMemBlock *relocated, bool branch);

void GenRelocateCodeX86Shared(void *buffer, CodeMemBlock *origin, CodeMemBlock *relocated, bool branch);

int GenRelocateSingleX86Insn(addr_t curr_orig_ip, addr_t curr_relo_ip, uint8_t *buffer_cursor, AssemblerBase *assembler,
                             CodeBufferBase *code_buffer, x86_insn_decode_t &insn, int8_t mode);