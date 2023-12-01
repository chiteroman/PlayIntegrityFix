#pragma once

#include "dobby/common.h"

#include "core/arch/x64/registers-x64.h"
#include "core/assembler/assembler.h"

#include "MemoryAllocator/CodeBuffer/code_buffer_x64.h"

#define IsInt8(imm) (-128 <= imm && imm <= 127)

enum ref_label_type_t { kDisp32_off_9 };

namespace zz {
namespace x64 {

constexpr Register VOLATILE_REGISTER = r11;

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
  explicit Immediate(int64_t imm) : value_(imm), value_size_(64) {
    if ((int64_t)(int8_t)imm == imm) {
      value_size_ = 8;
    } else if ((int64_t)(int16_t)imm == imm) {
      value_size_ = 8;
    } else if ((int64_t)(int32_t)imm == imm) {
      value_size_ = 32;
    } else {
      value_size_ = 64;
    }
  }

  explicit Immediate(int64_t imm, int size) : value_(imm), value_size_(size) {
  }

  int64_t value() const {
    return value_;
  }

  int size() const {
    return value_size_;
  }

private:
  const int64_t value_;

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
  uint8_t rex() const {
    return rex_;
  }

  inline uint8_t rex_b() const {
    return (rex_ & REX_B);
  }

  inline uint8_t rex_x() const {
    return (rex_ & REX_X);
  }

  inline uint8_t rex_r() const {
    return (rex_ & REX_R);
  }

  inline uint8_t rex_w() const {
    return (rex_ & REX_W);
  }

  uint8_t modrm() {
    return (encoding_at(0));
  }

  uint8_t mod() const {
    return (encoding_at(0) >> 6) & 3;
  }

  Register rm() const {
    int rm_rex = rex_b() << 3;
    return Register::from_code(rm_rex + (encoding_at(0) & 7));
  }

  ScaleFactor scale() const {
    return static_cast<ScaleFactor>((encoding_at(1) >> 6) & 3);
  }

  Register index() const {
    int index_rex = rex_x() << 2;
    return Register::from_code(index_rex + ((encoding_at(1) >> 3) & 7));
  }

  Register base() const {
    int base_rex = rex_b() << 3;
    return Register::from_code(base_rex + (encoding_at(1) & 7));
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
  Operand() : length_(0), rex_(REX_NONE) {
  } // Needed by subclass Address.

  void SetModRM(int mod, Register rm) {
    ASSERT((mod & ~3) == 0);

    if ((rm.code() > 7) && !((rm.Is(r12)) && (mod != 3))) {
      rex_ |= REX_B;
    }
    encoding_[0] = (mod << 6) | (rm.code() & 7);
    length_ = 1;
  }

  void SetSIB(ScaleFactor scale, Register index, Register base) {
    ASSERT(length_ == 1);
    ASSERT((scale & ~3) == 0);

    if (base.code() > 7) {
      ASSERT((rex_ & REX_B) == 0); // Must not have REX.B already set.
      rex_ |= REX_B;
    }
    if (index.code() > 7)
      rex_ |= REX_X;

    encoding_[1] = (scale << 6) | ((index.code() & 7) << 3) | (base.code() & 7);
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
  uint8_t rex_;
  uint8_t encoding_[6];
};

// ---

class Address : public Operand {
public:
  Address(Register base, int32_t disp) {
    int base_ = base.code();
    int rbp_ = rbp.code();
    int rsp_ = rsp.code();
    if ((disp == 0) && ((base_ & 7) != rbp_)) {
      SetModRM(0, base);
      if ((base_ & 7) == rsp_) {
        SetSIB(TIMES_1, rsp, base);
      }
    } else if (IsInt8(disp)) {
      SetModRM(1, base);
      if ((base_ & 7) == rsp_) {
        SetSIB(TIMES_1, rsp, base);
      }
      SetDisp8(disp);
    } else {
      SetModRM(2, base);
      if ((base_ & 7) == rsp_) {
        SetSIB(TIMES_1, rsp, base);
      }
      SetDisp32(disp);
    }
  }

  // This addressing mode does not exist.
  Address(Register base, Register r);

  Address(Register index, ScaleFactor scale, int32_t disp) {
    ASSERT(index.code() != rsp.code()); // Illegal addressing mode.
    SetModRM(0, rsp);
    SetSIB(scale, index, rbp);
    SetDisp32(disp);
  }

  // This addressing mode does not exist.
  Address(Register index, ScaleFactor scale, Register r);

  Address(Register base, Register index, ScaleFactor scale, int32_t disp) {
    ASSERT(index.code() != rsp.code()); // Illegal addressing mode.
    int rbp_ = rbp.code();
    if ((disp == 0) && ((base.code() & 7) != rbp_)) {
      SetModRM(0, rsp);
      SetSIB(scale, index, base);
    } else if (IsInt8(disp)) {
      SetModRM(1, rsp);
      SetSIB(scale, index, base);
      SetDisp8(disp);
    } else {
      SetModRM(2, rsp);
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
    if ((base.code() & 7) == rsp.code()) {
      SetSIB(TIMES_1, rsp, base);
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

  void EmitInt64(int64_t value) {
    buffer_->Emit64(value);
  }

  // ---

  // refer android_art
  uint8_t EmitOptionalRex(bool force, bool w, bool r, bool x, bool b) {
    // REX.WRXB
    // W - 64-bit operand
    // R - MODRM.reg
    // X - SIB.index
    // B - MODRM.rm/SIB.base

    uint8_t rex = force ? 0x40 : 0;
    if (w) {
      rex |= 0x48; // REX.W000
    }
    if (r) {
      rex |= 0x44; // REX.0R00
    }
    if (x) {
      rex |= 0x42; // REX.00X0
    }
    if (b) {
      rex |= 0x41; // REX.000B
    }
    if (rex != 0) {
      return rex;
    }
    return 0;
  }

  void Emit_64REX(uint8_t extra) {
    uint8_t rex = EmitOptionalRex(false, true, false, false, false);
    rex |= extra;
    if (rex)
      Emit1(rex);
  }

  void EmitREX_ExtraRegister(Register reg) {
    uint8_t rex = EmitOptionalRex(false, reg.size() == 64, reg.code() > 7, false, reg.code() > 7);
    if (rex)
      Emit1(rex);
  }

  void EmitREX_Register(Register reg) {
    uint8_t rex = EmitOptionalRex(false, reg.size() == 64, reg.code() > 7, false, false);
    if (rex)
      Emit1(rex);
  }

  void EmitREX_Register_Operand(Register reg, Operand &operand) {
    if (reg.size() != 64)
      UNIMPLEMENTED();
    uint8_t rex = operand.rex();
    rex |= EmitOptionalRex(true, reg.size() == 64, reg.code() > 7, false, false);
    if (rex != 0) {
      Emit1(rex);
    }
  }

  void EmitREX_Operand(Operand &operand) {
    uint8_t rex = operand.rex();
    rex |= REX_PREFIX;
    if (rex != 0) {
      Emit1(rex);
    }
  }

  // ---

  void EmitImmediate(Immediate imm, int imm_size) {
    if (imm_size == 8) {
      buffer_->Emit8((uint8_t)imm.value());
    } else if (imm_size == 32) {
      buffer_->Emit32((uint32_t)imm.value());
    } else if (imm_size == 64) {
      buffer_->Emit64((uint64_t)imm.value());
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
    EmitModRM(ModRM_Mod(modRM), reg.low_bits(), ModRM_RM(modRM));
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
    EmitOpcode(opcode | reg.low_bits());
  }

  // ---

  void pushfq() {
    Emit1(0x9C);
  }

  void jmp(Immediate imm);

  void sub(Register dst, Immediate imm) {
    EmitREX_Register(dst);

    EmitOpcode(0x81);

    Emit_OpEn_RegOperand_Immediate(0x5, dst, imm);
  }

  void add(Register dst, Immediate imm) {
    EmitREX_Register(dst);

    EmitOpcode(0x81);

    Emit_OpEn_RegOperand_Immediate(0x0, dst, imm);
  }

  // MOV RAX, 0x320
  // 48 c7 c0 20 03 00 00 (MI encoding)
  // 48 b8 20 03 00 00 00 00 00 00 (OI encoding)
  void mov(Register dst, const Immediate imm) {
    EmitREX_Register(dst);

    Emit_OpEn_OpcodeRegister_Immediate(0xb8, dst, imm);
  }

  void mov(Address dst, const Immediate imm) {
    EmitREX_Operand(dst);

    EmitOpcode(0xc7);

    Emit_OpEn_MemOperand_Immediate(0x0, dst, imm);
  }

  void mov(Register dst, Address src) {
    EmitREX_Register(dst);

    EmitOpcode(0x8B);

    Emit_OpEn_Register_MemOperand(dst, src);
  }

  void mov(Address dst, Register src) {
    EmitREX_Register_Operand(src, dst);

    EmitOpcode(0x89);

    Emit_OpEn_Register_MemOperand(src, dst);
  }

  void mov(Register dst, Register src) {
    EmitREX_Register(dst);

    Emit1(0x8B);

    Emit_OpEn_Register_RegOperand(dst, src);
  }

  void call(Address operand) {
    EmitREX_Operand(operand);

    EmitOpcode(0xFF);

    Emit_OpEn_MemOperand(0x2, operand);
  }

  void call(Immediate imm) {
    EmitOpcode(0xe8);

    EmitImmediate(imm, imm.size());
  }

  void call(Register reg) {
    EmitREX_Register(reg);

    EmitOpcode(0xFF);

    Emit_OpEn_RegOperand(0x2, reg);
  }

  void pop(Register reg) {
    EmitREX_ExtraRegister(reg);

    EmitOpcode_Register(0x58, reg);
  }

  void push(Register reg) {
    EmitREX_ExtraRegister(reg);

    EmitOpcode_Register(0x50, reg);
  }

  void ret() {
    EmitOpcode(0xc3);
  }
  void nop() {
    EmitOpcode(0x90);
  }
};

// ---

class TurboAssembler : public Assembler {
public:
  TurboAssembler(void *address) : Assembler(address) {
  }

  ~TurboAssembler() {
  }

  addr64_t CurrentIP();

  void CallFunction(ExternalReference function) {
#if 0
    mov(r11, Immediate((int64_t)function.address(), 64));
    call(r11);
#endif

    nop();
    MovRipToRegister(VOLATILE_REGISTER);
    call(Address(VOLATILE_REGISTER, INT32_MAX));
    {
      auto label = RelocLabel::withData((uint64_t)function.address());
      label->link_to(kDisp32_off_9, ip_offset());
      this->AppendRelocLabel(label);
    }
    nop();
  }

  void MovRipToRegister(Register dst) {
    call(Immediate(0, 32));
    pop(dst);
  }
};

} // namespace x64
} // namespace zz
