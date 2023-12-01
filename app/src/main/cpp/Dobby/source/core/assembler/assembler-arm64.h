#pragma once

#include "dobby/dobby_internal.h"

#include "core/arch/arm64/constants-arm64.h"
#include "core/arch/arm64/registers-arm64.h"
#include "core/assembler/assembler.h"

#include "MemoryAllocator/CodeBuffer/code_buffer_arm64.h"

#include "InstructionRelocation/arm64/inst_decode_encode_kit.h"

static inline uint16_t Low16Bits(uint32_t value) {
  return static_cast<uint16_t>(value & 0xffff);
}

static inline uint16_t High16Bits(uint32_t value) {
  return static_cast<uint16_t>(value >> 16);
}

static inline uint32_t Low32Bits(uint64_t value) {
  return static_cast<uint32_t>(value);
}

static inline uint32_t High32Bits(uint64_t value) {
  return static_cast<uint32_t>(value >> 32);
}

enum ref_label_type_t { kLabelImm19 };

namespace zz {
namespace arm64 {

constexpr Register TMP_REG_0 = X(ARM64_TMP_REG_NDX_0);

#define Rd(rd) (rd.code() << kRdShift)
#define Rt(rt) (rt.code() << kRtShift)
#define Rt2(rt) (rt.code() << kRt2Shift)
#define Rn(rn) (rn.code() << kRnShift)
#define Rm(rm) (rm.code() << kRmShift)

// ---

class Operand {
public:
  inline explicit Operand(int64_t imm)
      : immediate_(imm), reg_(InvalidRegister), shift_(NO_SHIFT), extend_(NO_EXTEND), shift_extent_imm_(0) {
  }
  inline Operand(Register reg, Shift shift = LSL, int32_t shift_imm = 0)
      : immediate_(0), reg_(reg), shift_(shift), extend_(NO_EXTEND), shift_extent_imm_(shift_imm) {
  }
  inline Operand(Register reg, Extend extend, int32_t shift_imm = 0)
      : immediate_(0), reg_(reg), shift_(NO_SHIFT), extend_(extend), shift_extent_imm_(shift_imm) {
  }

  bool IsImmediate() const {
    return reg_.Is(InvalidRegister);
  }
  bool IsShiftedRegister() const {
    return /* reg_.IsValid() && */ (shift_ != NO_SHIFT);
  }
  bool IsExtendedRegister() const {
    return /* reg_.IsValid() && */ (extend_ != NO_EXTEND);
  }

  Register reg() const {
    DCHECK((IsShiftedRegister() || IsExtendedRegister()));
    return reg_;
  }
  int64_t Immediate() const {
    return immediate_;
  }
  Shift shift() const {
    DCHECK(IsShiftedRegister());
    return shift_;
  }
  Extend extend() const {
    DCHECK(IsExtendedRegister());
    return extend_;
  }
  int32_t shift_extend_imm() const {
    return shift_extent_imm_;
  }

private:
  int64_t immediate_;

  Register reg_;

  Shift shift_;
  Extend extend_;
  int32_t shift_extent_imm_;
};

// ---

class MemOperand {
public:
  inline explicit MemOperand(Register base, int64_t offset = 0, AddrMode addrmode = Offset)
      : base_(base), regoffset_(InvalidRegister), offset_(offset), addrmode_(addrmode), shift_(NO_SHIFT),
        extend_(NO_EXTEND), shift_extend_imm_(0) {
  }

  inline explicit MemOperand(Register base, Register regoffset, Extend extend, unsigned extend_imm)
      : base_(base), regoffset_(regoffset), offset_(0), addrmode_(Offset), shift_(NO_SHIFT), extend_(extend),
        shift_extend_imm_(extend_imm) {
  }

  inline explicit MemOperand(Register base, Register regoffset, Shift shift = LSL, unsigned shift_imm = 0)
      : base_(base), regoffset_(regoffset), offset_(0), addrmode_(Offset), shift_(shift), extend_(NO_EXTEND),
        shift_extend_imm_(shift_imm) {
  }

  inline explicit MemOperand(Register base, const Operand &offset, AddrMode addrmode = Offset)
      : base_(base), regoffset_(InvalidRegister), addrmode_(addrmode) {
    if (offset.IsShiftedRegister()) {
      regoffset_ = offset.reg();
      shift_ = offset.shift();
      shift_extend_imm_ = offset.shift_extend_imm();

      extend_ = NO_EXTEND;
      offset_ = 0;
    } else if (offset.IsExtendedRegister()) {
      regoffset_ = offset.reg();
      extend_ = offset.extend();
      shift_extend_imm_ = offset.shift_extend_imm();

      shift_ = NO_SHIFT;
      offset_ = 0;
    }
  }

  const Register &base() const {
    return base_;
  }
  const Register &regoffset() const {
    return regoffset_;
  }
  int64_t offset() const {
    return offset_;
  }
  AddrMode addrmode() const {
    return addrmode_;
  }
  Shift shift() const {
    return shift_;
  }
  Extend extend() const {
    return extend_;
  }
  unsigned shift_extend_imm() const {
    return shift_extend_imm_;
  }

  bool IsImmediateOffset() const {
    return (addrmode_ == Offset);
  }
  bool IsRegisterOffset() const {
    return (addrmode_ == Offset);
  }
  bool IsPreIndex() const {
    return addrmode_ == PreIndex;
  }
  bool IsPostIndex() const {
    return addrmode_ == PostIndex;
  }

private:
  Register base_;
  Register regoffset_;

  int64_t offset_;

  Shift shift_;
  Extend extend_;
  uint32_t shift_extend_imm_;

  AddrMode addrmode_;
};

// ---

class OpEncode {
public:
  static int32_t sf(const Register &reg, int32_t op) {
    return (op | sf(reg));
  }

  // register operation size, 32 bits or 64 bits
  static int32_t sf(const Register &reg) {
    if (reg.Is64Bits())
      return LeftShift(1, 1, 31);
    return 0;
  }

  static int32_t V(const Register &reg, int32_t op) {
    return (op | V(reg));
  }

  // register type, SIMD_FD register or general register
  static int32_t V(const Register &reg) {
    if (reg.IsVRegister())
      return LeftShift(1, 1, 26);
    return 0;
  }

  // load or store
  static int32_t L(bool load_or_store) {
    if (load_or_store) {
      return LeftShift(1, 1, 22);
    }
    return 0;
  }

  // shift type
  static int32_t shift(Shift shift) {
    return LeftShift(shift, 2, 22);
  }

  // LogicalImmeidate
  static int32_t EncodeLogicalImmediate(const Register &rd, const Register &rn, const Operand &operand) {
    int64_t imm = operand.Immediate();
    int32_t N, imms, immr;
    immr = bits(imm, 0, 5);
    imms = bits(imm, 6, 11);
    N = bit(imm, 12);

    return (sf(rd) | LeftShift(immr, 6, 16) | LeftShift(imms, 6, 10) | Rd(rd) | Rn(rn));
  }

  // LogicalShift
  static int32_t EncodeLogicalShift(const Register &rd, const Register &rn, const Operand &operand) {
    return (sf(rd) | shift(operand.shift()) | Rm(operand.reg()) | LeftShift(operand.shift_extend_imm(), 6, 10) |
            Rn(rn) | Rd(rd));
  }

  // LoadStore
  static int32_t LoadStorePair(LoadStorePairOp op, CPURegister rt, CPURegister rt2, const MemOperand &addr) {
    int32_t scale = 2;
    int32_t opc = 0;
    int imm7;
    opc = bits(op, 30, 31);
    if (rt.IsRegister()) {
      scale += bit(opc, 1);
    } else if (rt.IsVRegister()) {
      scale += opc;
    }

    imm7 = (int)(addr.offset() >> scale);
    return LeftShift(imm7, 7, 15);
  }

  // scale
  static int32_t scale(int32_t op) {
    int scale = 0;
    if ((op & LoadStoreUnsignedOffsetFixed) == LoadStoreUnsignedOffsetFixed) {
      scale = bits(op, 30, 31);
    }
    return scale;
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
  void SetRealizedAddress(void *address) {
    DCHECK_EQ(0, reinterpret_cast<uint64_t>(address) % 4);
    AssemblerBase::SetRealizedAddress(address);
  }

  void Emit(uint32_t value) {
    buffer_->Emit32(value);
  }

  void EmitInt64(int64_t value) {
    buffer_->Emit64(value);
  }

  void bind(Label *label);

  void nop() {
    Emit(0xD503201F);
  }

  void brk(int code) {
    Emit(BRK | LeftShift(code, 16, 5));
  }

  void ret() {
    Emit(0xD65F03C0);
  }

  void adrp(const Register &rd, int64_t imm) {
    DCHECK(rd.Is64Bits());
    DCHECK((abs(imm) >> 12) < (1 << 21));

    uint32_t immlo = LeftShift(bits(imm >> 12, 0, 1), 2, 29);
    uint32_t immhi = LeftShift(bits(imm >> 12, 2, 20), 19, 5);
    Emit(ADRP | Rd(rd) | immlo | immhi);
  }

  void add(const Register &rd, const Register &rn, int64_t imm) {
    if (rd.Is64Bits() && rn.Is64Bits())
      AddSubImmediate(rd, rn, Operand(imm), OPT_X(ADD, imm));
    else
      AddSubImmediate(rd, rn, Operand(imm), OPT_W(ADD, imm));
  }

  void adds(const Register &rd, const Register &rn, int64_t imm) {
    UNREACHABLE();
  }
  void sub(const Register &rd, const Register &rn, int64_t imm) {
    if (rd.Is64Bits() && rn.Is64Bits())
      AddSubImmediate(rd, rn, Operand(imm), OPT_X(SUB, imm));
    else
      AddSubImmediate(rd, rn, Operand(imm), OPT_W(SUB, imm));
  }
  void subs(const Register &rd, const Register &rn, int64_t imm) {
    UNREACHABLE();
  }

  void b(int64_t imm) {
    int32_t imm26 = bits(imm >> 2, 0, 25);

    Emit(B | imm26);
  }

  void b(Label *label) {
    int offset = LinkAndGetByteOffsetTo(label);
    b(offset);
  }

  void br(Register rn) {
    Emit(BR | Rn(rn));
  }

  void blr(Register rn) {
    Emit(BLR | Rn(rn));
  }

  void ldr(Register rt, int64_t imm) {
    LoadRegLiteralOp op;
    switch (rt.type()) {
    case CPURegister::kRegister_32:
      op = OPT_W(LDR, literal);
      break;
    case CPURegister::kRegister_X:
      op = OPT_X(LDR, literal);
      break;
    case CPURegister::kSIMD_FP_Register_S:
      op = OPT_S(LDR, literal);
      break;
    case CPURegister::kSIMD_FP_Register_D:
      op = OPT_D(LDR, literal);
      break;
    case CPURegister::kSIMD_FP_Register_Q:
      op = OPT_Q(LDR, literal);
      break;
    default:
      UNREACHABLE();
      break;
    }
    EmitLoadRegLiteral(op, rt, imm);
  }

  void ldr(const CPURegister &rt, const MemOperand &src) {
    LoadStore(OP_X(LDR), rt, src);
  }

  void str(const CPURegister &rt, const MemOperand &src) {
    LoadStore(OP_X(STR), rt, src);
  }

  void ldp(const Register &rt, const Register &rt2, const MemOperand &src) {
    if (rt.type() == Register::kSIMD_FP_Register_128) {
      LoadStorePair(OP_Q(LDP), rt, rt2, src);
    } else if (rt.type() == Register::kRegister_X) {
      LoadStorePair(OP_X(LDP), rt, rt2, src);
    } else {
      UNREACHABLE();
    }
  }

  void stp(const Register &rt, const Register &rt2, const MemOperand &dst) {
    if (rt.type() == Register::kSIMD_FP_Register_128) {
      LoadStorePair(OP_Q(STP), rt, rt2, dst);
    } else if (rt.type() == Register::kRegister_X) {
      LoadStorePair(OP_X(STP), rt, rt2, dst);
    } else {
      UNREACHABLE();
    }
  }

  void mov(const Register &rd, const Register &rn) {
    if ((rd.Is(SP)) || (rn.Is(SP))) {
      add(rd, rn, 0);
    } else {
      if (rd.Is64Bits())
        orr(rd, xzr, Operand(rn));
      else
        orr(rd, wzr, Operand(rn));
    }
  }
  void movk(const Register &rd, uint64_t imm, int shift = -1) {
    // Move and keep.
    MoveWide(rd, imm, shift, MOVK);
  }
  void movn(const Register &rd, uint64_t imm, int shift = -1) {
    // Move with non-zero.
    MoveWide(rd, imm, shift, MOVN);
  }
  void movz(const Register &rd, uint64_t imm, int shift = -1) {
    // Move with zero.
    MoveWide(rd, imm, shift, MOVZ);
  }

  void orr(const Register &rd, const Register &rn, const Operand &operand) {
    Logical(rd, rn, operand, ORR);
  }

private:
  // label helpers.
  static constexpr int kStartOfLabelLinkChain = 0;
  int LinkAndGetByteOffsetTo(Label *label);

  // load helpers.
  void EmitLoadRegLiteral(LoadRegLiteralOp op, CPURegister rt, int64_t imm) {
    const int32_t encoding = op | LeftShift(imm, 26, 5) | Rt(rt);
    Emit(encoding);
  }

  void LoadStore(LoadStoreOp op, CPURegister rt, const MemOperand &addr) {
    int64_t imm12 = addr.offset();
    if (addr.IsImmediateOffset()) {
      // TODO: check Scaled ???
      imm12 = addr.offset() >> OpEncode::scale(LoadStoreUnsignedOffsetFixed | op);
      Emit(LoadStoreUnsignedOffsetFixed | op | LeftShift(imm12, 12, 10) | Rn(addr.base()) | Rt(rt));
    } else if (addr.IsRegisterOffset()) {
      UNREACHABLE();
    } else {
      // pre-index & post-index
      UNREACHABLE();
    }
  }

  void LoadStorePair(LoadStorePairOp op, CPURegister rt, CPURegister rt2, const MemOperand &addr) {
    int32_t combine_fields_op = OpEncode::LoadStorePair(op, rt, rt2, addr) | Rt2(rt2) | Rn(addr.base()) | Rt(rt);
    int32_t addrmodeop;

    if (addr.IsImmediateOffset()) {
      addrmodeop = LoadStorePairOffsetFixed;
    } else {
      if (addr.IsPreIndex()) {
        addrmodeop = LoadStorePairPreIndexFixed;
      } else {
        addrmodeop = LoadStorePairPostIndexFixed;
      }
    }
    Emit(op | addrmodeop | combine_fields_op);
  }

  void MoveWide(Register rd, uint64_t imm, int shift, MoveWideImmediateOp op) {
    if (shift > 0)
      shift /= 16;
    else
      shift = 0;

    int32_t imm16 = LeftShift(imm, 16, 5);
    Emit(MoveWideImmediateFixed | op | OpEncode::sf(rd) | LeftShift(shift, 2, 21) | imm16 | Rd(rd));
  }

  void AddSubImmediate(const Register &rd, const Register &rn, const Operand &operand, AddSubImmediateOp op) {
    if (operand.IsImmediate()) {
      int64_t immediate = operand.Immediate();
      int32_t imm12 = LeftShift(immediate, 12, 10);
      Emit(op | Rd(rd) | Rn(rn) | imm12);
    } else {
      UNREACHABLE();
    }
  }

  void Logical(const Register &rd, const Register &rn, const Operand &operand, LogicalOp op) {
    if (operand.IsImmediate()) {
      LogicalImmediate(rd, rn, operand, op);
    } else {
      LogicalShift(rd, rn, operand, op);
    }
  }
  void LogicalImmediate(const Register &rd, const Register &rn, const Operand &operand, LogicalOp op) {
    int32_t combine_fields_op = OpEncode::EncodeLogicalImmediate(rd, rn, operand);
    Emit(op | combine_fields_op);
  }
  void LogicalShift(const Register &rd, const Register &rn, const Operand &operand, LogicalOp op) {
    int32_t combine_fields_op = OpEncode::EncodeLogicalShift(rd, rn, operand);
    Emit(op | LogicalShiftedFixed | combine_fields_op);
  }
};

// ---

class TurboAssembler : public Assembler {
public:
  TurboAssembler(void *address) : Assembler(address) {
  }

  ~TurboAssembler() {
  }

  void CallFunction(ExternalReference function) {
    Mov(TMP_REG_0, (uint64_t)function.address());
    blr(TMP_REG_0);
  }

  void Ldr(Register rt, AssemblerPseudoLabel *label) {
    if (label->pos()) {
      int offset = label->pos() - buffer_->GetBufferSize();
      ldr(rt, offset);
    } else {
      label->link_to(kLabelImm19, buffer_->GetBufferSize());
      ldr(rt, 0);
    }
  }

  void Mov(Register rd, uint64_t imm) {
    const uint32_t w0 = Low32Bits(imm);
    const uint32_t w1 = High32Bits(imm);
    const uint16_t h0 = Low16Bits(w0);
    const uint16_t h1 = High16Bits(w0);
    const uint16_t h2 = Low16Bits(w1);
    const uint16_t h3 = High16Bits(w1);
    movz(rd, h0, 0);
    movk(rd, h1, 16);
    movk(rd, h2, 32);
    movk(rd, h3, 48);
  }

  void AdrpAdd(Register rd, uint64_t from, uint64_t to) {
    uint64_t from_PAGE = ALIGN(from, 0x1000);
    uint64_t to_PAGE = ALIGN(to, 0x1000);
    uint64_t to_PAGEOFF = (uint64_t)to % 0x1000;

    adrp(rd, to_PAGE - from_PAGE);
    add(rd, rd, to_PAGEOFF);
  }
};

} // namespace arm64
} // namespace zz