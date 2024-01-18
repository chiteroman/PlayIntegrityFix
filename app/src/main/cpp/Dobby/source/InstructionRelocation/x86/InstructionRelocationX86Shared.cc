#include "platform_macro.h"

#if defined(TARGET_ARCH_IA32) || defined(TARGET_ARCH_X64)

#include "dobby_internal.h"

#include "InstructionRelocation/x86/InstructionRelocationX86.h"
#include "InstructionRelocation/x86/x86_insn_decode/x86_insn_decode.h"
#include "MemoryAllocator/NearMemoryAllocator.h"

using namespace zz::x86;

// x64 jmp absolute address
void codegen_x64_jmp_absolute_addr(CodeBufferBase *buffer, addr_t target) {
  // jmp *(rip)
  buffer->Emit8(0xFF);
  buffer->Emit8(0x25); // ModR/M: 00 100 101
  buffer->Emit32(0x00);
  // .long target
  buffer->Emit64(target);
}

int GenRelocateSingleX86Insn(addr_t curr_orig_ip, addr_t curr_relo_ip, uint8_t *buffer_cursor,
                             CodeBufferBase *code_buffer, x86_insn_decode_t &insn, int8_t mode) {
#define __ code_buffer->

  int relocated_insn_len = -1;

  x86_options_t conf = {0};
  conf.mode = mode;

  // decode x86 insn
  x86_insn_decode(&insn, (uint8_t *)buffer_cursor, &conf);

  // x86 ip register == next instruction
  curr_orig_ip = curr_orig_ip + insn.length;

  int last_relo_offset = code_buffer->GetBufferSize();
  if (insn.primary_opcode >= 0x70 && insn.primary_opcode <= 0x7F) { // jc rel8
    DLOG(0, "[x86 relo] %p: jc rel8", buffer_cursor);

    int8_t offset = insn.immediate;

    uint8_t opcode = 0x80 | (insn.primary_opcode & 0x0f);

#if defined(TARGET_ARCH_IA32)
    curr_relo_ip = curr_relo_ip + 6;
    int32_t new_offset = (int32_t)(curr_orig_ip + offset - curr_relo_ip);

    __ Emit8(0x0F);
    __ Emit8(opcode);
    __ Emit32(new_offset);
#else
    curr_relo_ip = curr_relo_ip + 2 + 2 + 6 + 8;
    uint64_t orig_insn_ref_addr = curr_orig_ip + offset;

    // jcc
    __ Emit8(opcode);
    __ Emit8(2);

    // jmp
    __ Emit8(0xEB);
    __ Emit8(6 + 8);

    // jmp abs addr
    codegen_x64_jmp_absolute_addr(code_buffer, orig_insn_ref_addr);
#endif

  } else if (mode == 64 && (insn.flags & X86_INSN_DECODE_FLAG_IP_RELATIVE) &&
             (insn.operands[1].mem.base == RIP)) { // RIP
    DLOG(0, "[x86 relo] %p: rip", buffer_cursor);

    curr_relo_ip = curr_relo_ip + 6 + 8;

    addr_t rip_insn_seq_addr = 0;
    {
      int32_t orig_disp = insn.operands[1].mem.disp;
      addr_t orig_rip_ref_addr = curr_orig_ip + orig_disp;

      uint32_t jmp_near_range = (uint32_t)2 * 1024 * 1024 * 1024;
      auto rip_insn_seq = (addr_t)NearMemoryAllocator::SharedAllocator()->allocateNearExecMemory(
          insn.length + 6 + 8, orig_rip_ref_addr, jmp_near_range);

      rip_insn_seq_addr = rip_insn_seq;

      auto rip_insn_seq_buffer = CodeBufferBase();
#define ___ rip_insn_seq_buffer.

      auto rip_insn_req_ip = rip_insn_seq;
      rip_insn_req_ip = rip_insn_req_ip + insn.length; // next insn addr
      int32_t new_disp = (int32_t)(orig_rip_ref_addr - rip_insn_req_ip);

      // keep orig insn opcode
      ___ EmitBuffer(buffer_cursor, insn.displacement_offset);
      ___ Emit32(new_disp);
      // keep orig insn immediate
      if (insn.immediate_offset) {
        ___ EmitBuffer((buffer_cursor + insn.immediate_offset), insn.length - insn.immediate_offset);
      }

      // jmp *(rip) => back to relo process
      codegen_x64_jmp_absolute_addr(&rip_insn_seq_buffer, curr_relo_ip);

      DobbyCodePatch((void *)rip_insn_seq, rip_insn_seq_buffer.GetBuffer(), rip_insn_seq_buffer.GetBufferSize());
    }

    // jmp *(rip) => jmp to [rip insn seq]
    __ Emit8(0xFF);
    __ Emit8(0x25); // ModR/M: 00 100 101
    __ Emit32(0);
    __ Emit64(rip_insn_seq_addr);
  } else if (insn.primary_opcode == 0xEB) { // jmp rel8
    DLOG(0, "[x86 relo] %p: jmp rel8", buffer_cursor);

    int8_t offset = insn.immediate;

#if defined(TARGET_ARCH_IA32)
    curr_relo_ip = curr_relo_ip + 5;
    int32_t new_offset = (int32_t)(curr_orig_ip + offset - curr_relo_ip);

    __ Emit8(0xE9);
    __ Emit32(new_offset);
#else
    curr_relo_ip = curr_relo_ip + 6 + 8;
    uint64_t orig_insn_ref_addr = curr_orig_ip + offset;

    // jmp *(rip)
    codegen_x64_jmp_absolute_addr(code_buffer, orig_insn_ref_addr);
#endif
  } else if (insn.primary_opcode == 0xE8 || insn.primary_opcode == 0xE9) { // call or jmp rel32
    DLOG(0, "[x86 relo] %p:jmp or call rel32", buffer_cursor);

    int32_t offset = insn.immediate;

    assert(insn.immediate_offset == 1);

#if defined(TARGET_ARCH_IA32)
    curr_relo_ip = curr_relo_ip + 5;
    int32_t new_offset = (int32_t)(curr_orig_ip + offset - curr_relo_ip);

    __ EmitBuffer(buffer_cursor, insn.immediate_offset);
    __ Emit32(new_offset);
#else
    curr_relo_ip = curr_relo_ip + 6 + 8;
    uint64_t orig_insn_ref_addr = curr_orig_ip + offset;

    // jmp *(rip)
    __ Emit8(0xFF);
    if (insn.primary_opcode == 0xE8)
      __ Emit8(0x15); // ModR/M: 00 010 101
    else
      __ Emit8(0x25); // ModR/M: 00 100 101
    __ Emit32(0);
    __ Emit64(orig_insn_ref_addr);
#endif
  } else if (insn.primary_opcode >= 0xE0 && insn.primary_opcode <= 0xE2) { // LOOPNZ/LOOPZ/LOOP/JECXZ
    // LOOP/LOOPcc
    UNIMPLEMENTED();
  } else if (insn.primary_opcode == 0xE3) {
    // JCXZ JCEXZ JCRXZ
    UNIMPLEMENTED();
  } else {
    __ EmitBuffer(buffer_cursor, insn.length);
  }

  // insn -> relocated insn
  {
    int relo_offset = code_buffer->GetBufferSize();
    int relo_len = relo_offset - last_relo_offset;
    DLOG(0, "insn -> relocated insn: %d -> %d", insn.length, relo_len);
  }
  return relocated_insn_len;
}

void GenRelocateCodeX86Shared(void *buffer, CodeMemBlock *origin, CodeMemBlock *relocated, bool branch) {
  int expected_relocated_mem_size = 32;
x86_try_again:
  if (!relocated->addr) {
    auto relocated_mem = MemoryAllocator::SharedAllocator()->allocateExecMemory(expected_relocated_mem_size);
    if (relocated_mem == nullptr) {
      return;
    }
    relocated->reset((addr_t)relocated_mem, expected_relocated_mem_size);
  }

  int ret = GenRelocateCodeFixed(buffer, origin, relocated, branch);
  if (ret != RT_SUCCESS) {
    const int step_size = 16;
    expected_relocated_mem_size += step_size;
    relocated->reset(0, 0);

    goto x86_try_again;
  }
}

#endif
