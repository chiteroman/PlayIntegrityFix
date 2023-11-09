#ifndef CODE_BUFFER_ARM_H
#define CODE_BUFFER_ARM_H

#include "MemoryAllocator/CodeBuffer/CodeBufferBase.h"

typedef int32_t arm_inst_t;
typedef int16_t thumb1_inst_t;
typedef int32_t thumb2_inst_t;

class CodeBuffer : public CodeBufferBase {
  enum ExecuteState { ARMExecuteState, ThumbExecuteState };

public:
  CodeBuffer() : CodeBufferBase() {
  }

public:
  arm_inst_t LoadARMInst(uint32_t offset) {
    return *reinterpret_cast<arm_inst_t *>(GetBuffer() + offset);
  }

  thumb1_inst_t LoadThumb1Inst(uint32_t offset) {
    return *reinterpret_cast<thumb1_inst_t *>(GetBuffer() + offset);
  }

  thumb2_inst_t LoadThumb2Inst(uint32_t offset) {
    return *reinterpret_cast<thumb2_inst_t *>(GetBuffer() + offset);
  }

  void RewriteAddr(uint32_t offset, addr32_t addr) {
    memcpy(GetBuffer() + offset, &addr, sizeof(addr));
  }

  void RewriteARMInst(uint32_t offset, arm_inst_t instr) {
    *reinterpret_cast<arm_inst_t *>(GetBuffer() + offset) = instr;
  }

  void RewriteThumb1Inst(uint32_t offset, thumb1_inst_t instr) {
    *reinterpret_cast<thumb1_inst_t *>(GetBuffer() + offset) = instr;
  }

  void RewriteThumb2Inst(uint32_t offset, thumb2_inst_t instr) {
    memcpy(GetBuffer() + offset, &instr, sizeof(instr));
  }

  void EmitARMInst(arm_inst_t instr) {
    Emit(instr);
  }

  void EmitThumb1Inst(thumb1_inst_t instr) {
    Emit(instr);
  }

  void EmitThumb2Inst(thumb2_inst_t instr) {
    Emit(instr);
  }
};

#endif