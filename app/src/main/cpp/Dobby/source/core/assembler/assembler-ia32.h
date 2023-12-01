#pragma once

#include "dobby/common.h"

#include "core/arch/x86/registers-x86.h"
#include "core/assembler/assembler.h"

#include "MemoryAllocator/CodeBuffer/code_buffer_x86.h"

#define IsInt8(imm) (-128 <= imm && imm <= 127)

enum ref_label_type_t { kDisp32_off_7 };

namespace zz {
namespace x86 {

constexpr Register VOLATILE_REGISTER = eax;

#define ModRM_Mod(byte) ((byte & 0b11000000) >> 6)
#define ModRM_RegOpcode(byte) ((byte & 0b00111000) >> 3)
#define ModRM_RM(byte) (byte & 0b00000111)

typedef union _ModRM {
  byte_t ModRM;
  struct {
    byte_t RM : 3;
    byte_t RegOpcode : 3;
    byte_t Mod : 2;
  };
} ModRM;

// ---

class Immediate {
public:
  explicit Immediate(int32_t imm) : value_(imm), value_size_(32) {
    if ((int32_t)(int8_t)imm == imm) {
      value_size_ = 8;
    } else if ((int32_t)(int16_t)imm == imm) {
      value_size_ = 16;
    } else {
      value_size_ = 32;
    }
  }

  explicit Immediate(int32_t imm, int size) : value_(imm), value_size_(size) {
  }

  int32_t value() const {
    return value_;
  }

  int size() const {
    return value_size_;
  }

private:
  const int32_t value_;

  int value_size_;
};

// ---

class Operand {
public:
  // [base]
  Operand(Register base);

  // [base + disp/r]
  Operand(Register base, int32_t disp);

  // [base + index*scale + disp/r]
  Operand(Register base, Register index, ScaleFactor scale, int32_t disp);

  // [index*scale + disp/r]
  Operand(Register index, ScaleFactor scale, int32_t disp);

public: // Getter and Setter
  uint8_t modrm() {
    return (encoding_at(0));
  }

  uint8_t mod() const {
    return (encoding_at(0) >> 6) & 3;
  }

  Register rm() const {
    return Register::from_code(encoding_at(0) & 7);
  }

  ScaleFactor scale() const {
    return static_cast<ScaleFactor>((encoding_at(1) >> 6) & 3);
  }

  Register index() const {
    return Register::from_code((encoding_at(1) >> 3) & 7);
  }

  Register base() const {
    return Register::from_code(encoding_at(1) & 7);
  }

  int8_t disp8() const {
    ASSERT(length_ >= 2);
    return static_cast<int8_t>(encoding_[length_ - 1]);
  }

  int32_t disp32() const {
    ASSERT(length_ >= 5);
    return static_cast<int32_t>(encoding_[length_ - 4]);
  }

protected:
  Operand() : length_(0) {
  }

  void SetModRM(int mod, Register rm) {
    ASSERT((mod & ~3) == 0);
    encoding_[0] = (mod << 6) | rm.code();
    length_ = 1;
  }

  void SetSIB(ScaleFactor scale, Register index, Register base) {
    ASSERT(length_ == 1);
    ASSERT((scale & ~3) == 0);
    encoding_[1] = (scale << 6) | (index.code() << 3) | base.code();
    length_ = 2;
  }

  void SetDisp8(int8_t disp) {
    ASSERT(length_ == 1 || length_ == 2);
    encoding_[length_++] = static_cast<uint8_t>(disp);
  }

  void SetDisp32(int32_t disp) {
    ASSERT(length_ == 1 || length_ == 2);
    *(int32_t *)&encoding_[length_] = disp;
    length_ += sizeof(disp);
  }

private:
  // explicit Operand(Register reg) : rex_(REX_NONE) { SetModRM(3, reg); }

  // Get the operand encoding byte at the given index.
  uint8_t encoding_at(intptr_t index) const {
    ASSERT(index >= 0 && index < length_);
    return encoding_[index];
  }

public:
  uint8_t length_;
  uint8_t encoding_[6];
};

// ---

class Address : public Operand {
public:
  Address(Register base, int32_t disp) {
    int base_ = base.code();
    int ebp_ = ebp.code();
    int esp_ = esp.code();
    if ((disp == 0) && (base_ != ebp_)) {
      SetModRM(0, base);
      if (base_ == esp_)
        SetSIB(TIMES_1, esp, base);
    } else if (disp >= -128 && disp <= 127) {
      SetModRM(1, base);
      if (base_ == esp_)
        SetSIB(TIMES_1, esp, base);
      SetDisp8(disp);
    } else {
      SetModRM(2, base);
      if (base_ == esp_)
        SetSIB(TIMES_1, esp, base);
      SetDisp32(disp);
    }
  }

  // This addressing mode does not exist.
  Address(Register base, Register r);

  Address(Register index, ScaleFactor scale, int32_t disp) {
    ASSERT(index.code() != rsp.code()); // Illegal addressing mode.
    SetModRM(0, esp);
    SetSIB(scale, index, ebp);
    SetDisp32(disp);
  }

  // This addressing mode does not exist.
  Address(Register index, ScaleFactor scale, Register r);

  Address(Register base, Register index, ScaleFactor scale, int32_t disp) {
    ASSERT(index.code() != rsp.code()); // Illegal addressing mode.
    int rbp_ = ebp.code();
    if ((disp == 0) && ((base.code() & 7) != rbp_)) {
      SetModRM(0, esp);
      SetSIB(scale, index, base);
    } else if (disp >= -128 && disp <= 127) {
      SetModRM(1, esp);
      SetSIB(scale, index, base);
      SetDisp8(disp);
    } else {
      SetModRM(2, esp);
      SetSIB(scale, index, base);
      SetDisp32(disp);
    }
  }

  // This addressing mode does not exist.
  Address(Register base, Register index, ScaleFactor scale, Register r);

private:
  Address(Register base, int32_t disp, bool fixed) {
    ASSERT(fixed);

    SetModRM(2, base);
    if (base.code() == esp.code()) {
      SetSIB(TIMES_1, esp, base);
    }
    SetDisp32(disp);
  }
};

// ---

class Assembler : public AssemblerBase {
public:
  Assembler(void *address) : AssemblerBase(address) {
    buffer_ = new CodeBuffer();
  }
  ~Assembler() {
    if (buffer_)
      delete buffer_;
    buffer_ = NULL;
  }

public:
  void Emit1(byte_t val) {
    buffer_->Emit8(val);
  }

  void Emit(int32_t value) {
    buffer_->Emit32(value);
  }

  // ---

  void EmitImmediate(Immediate imm, int imm_size) {
    if (imm_size == 8) {
      buffer_->Emit8((uint8_t)imm.value());
    } else if (imm_size == 32) {
      buffer_->Emit32((uint32_t)imm.value());
    } else {
      UNREACHABLE();
    }
  }

  // ---

  // ATTENTION:
  // ModR/M == 8 registers and 24 addressing mode

  void Emit_OpEn_Register_MemOperand(Register dst, Address &operand) {
    EmitModRM_Update_Register(operand.modrm(), dst);
    buffer_->EmitBuffer(&operand.encoding_[1], operand.length_ - 1);
  }

  void Emit_OpEn_Register_RegOperand(Register dst, Register src) {
    EmitModRM_Register_Register(dst, src);
  }

  void Emit_OpEn_MemOperand_Immediate(uint8_t extra_opcode, Address &operand, Immediate imm) {
    EmitModRM_Update_ExtraOpcode(operand.modrm(), extra_opcode);
    buffer_->EmitBuffer(&operand.encoding_[1], operand.length_ - 1);
    EmitImmediate(imm, imm.size());
  }

  void Emit_OpEn_RegOperand_Immediate(uint8_t extra_opcode, Register reg, Immediate imm) {
    EmitModRM_ExtraOpcode_Register(extra_opcode, reg);
    EmitImmediate(imm, imm.size());
  }

  void Emit_OpEn_MemOperand(uint8_t extra_opcode, Address &operand) {
    EmitModRM_Update_ExtraOpcode(operand.modrm(), extra_opcode);
    buffer_->EmitBuffer(&operand.encoding_[1], operand.length_ - 1);
  }

  void Emit_OpEn_RegOperand(uint8_t extra_opcode, Register reg) {
    EmitModRM_ExtraOpcode_Register(extra_opcode, reg);
  }

  // Encoding: OI
  void Emit_OpEn_OpcodeRegister_Immediate(uint8_t opcode, Register dst, Immediate imm) {
    EmitOpcode_Register(opcode, dst);
    EmitImmediate(imm, imm.size());
  }

  // ---

  inline void EmitModRM(uint8_t Mod, uint8_t RegOpcode, uint8_t RM) {
    uint8_t ModRM = 0;
    ModRM |= Mod << 6;
    ModRM |= RegOpcode << 3;
    ModRM |= RM;
    Emit1(ModRM);
  }

  void EmitModRM_ExtraOpcode_Register(uint8_t extra_opcode, Register reg) {
    EmitModRM(0b11, extra_opcode, reg.code());
  }

  void EmitModRM_Register_Register(Register reg1, Register reg2) {
    EmitModRM(0b11, reg1.code(), reg2.code());
  }

  // update operand's ModRM
  void EmitModRM_Update_Register(uint8_t modRM, Register reg) {
    EmitModRM(ModRM_Mod(modRM), reg.code(), ModRM_RM(modRM));
  }

  // update operand's ModRM
  void EmitModRM_Update_ExtraOpcode(uint8_t modRM, uint8_t extra_opcode) {
    EmitModRM(ModRM_Mod(modRM), extra_opcode, ModRM_RM(modRM));
  }

  // ---

  void EmitOpcode(uint8_t opcode) {
    Emit1(opcode);
  }

  void EmitOpcode_Register(uint8_t opcode, Register reg) {
    EmitOpcode(opcode | reg.code());
  }

  // ---

  void pushfq() {
    Emit1(0x9C);
  }

  void jmp(Immediate imm);

  void sub(Register dst, Immediate imm) {
    DCHECK_EQ(dst.size(), 32);

    EmitOpcode(0x81);

    Emit_OpEn_RegOperand_Immediate(0x5, dst, imm);
  }

  void add(Register dst, Immediate imm) {
    DCHECK_EQ(dst.size(), 32);

    EmitOpcode(0x81);

    Emit_OpEn_RegOperand_Immediate(0x0, dst, imm);
  }

  // MOV RAX, 0x320
  // 48 c7 c0 20 03 00 00 (MI encoding)
  // 48 b8 20 03 00 00 00 00 00 00 (OI encoding)
  void mov(Register dst, const Immediate imm) {
    Emit_OpEn_OpcodeRegister_Immediate(0xb8, dst, imm);
  }

  void mov(Address dst, const Immediate imm) {
    EmitOpcode(0xc7);

    Emit_OpEn_MemOperand_Immediate(0x0, dst, imm);
  }

  void mov(Register dst, Address src) {
    EmitOpcode(0x8B);

    Emit_OpEn_Register_MemOperand(dst, src);
  }

  void mov(Address dst, Register src) {
    EmitOpcode(0x89);

    Emit_OpEn_Register_MemOperand(src, dst);
  }

  void mov(Register dst, Register src) {
    Emit1(0x8B);

    Emit_OpEn_Register_RegOperand(dst, src);
  }

  void call(Address operand) {
    EmitOpcode(0xFF);

    Emit_OpEn_MemOperand(0x2, operand);
  }

  void call(Immediate imm) {
    EmitOpcode(0xe8);

    EmitImmediate(imm, imm.size());
  }

  void call(Register reg) {
    EmitOpcode(0xFF);

    Emit_OpEn_RegOperand(0x2, reg);
  }

  void pop(Register reg) {
    EmitOpcode_Register(0x58, reg);
  }

  void push(Register reg) {
    EmitOpcode_Register(0x50, reg);
  }

  void ret() {
    EmitOpcode(0xc3);
  }
  void nop() {
    EmitOpcode(0x90);
  }
};

class TurboAssembler : public Assembler {
public:
  TurboAssembler(void *address) : Assembler(address) {
  }

  ~TurboAssembler() {
  }

  addr32_t CurrentIP();

  void CallFunction(ExternalReference function) {
    nop();
    MovRipToRegister(VOLATILE_REGISTER);
    call(Address(VOLATILE_REGISTER, INT32_MAX));
    {
      auto label = RelocLabel::withData(function.address());
      label->link_to(kDisp32_off_7, ip_offset());
      AppendRelocLabel(label);
    }
    nop();
  }

  void MovRipToRegister(Register dst) {
    call(Immediate(0, 32));
    pop(dst);
  }
};

} // namespace x86
} // namespace zz