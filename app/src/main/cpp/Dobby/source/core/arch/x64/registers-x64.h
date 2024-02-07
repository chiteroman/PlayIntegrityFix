#ifndef ARCH_X64_REGISTERS
#define ARCH_X64_REGISTERS

#include "core/arch/x64/constants-x64.h"
#include "core/arch/Cpu.h"

namespace zz {
namespace x64 {

#define GENERAL_REGISTERS(V)                                                                                           \
  V(rax)                                                                                                               \
  V(rcx)                                                                                                               \
  V(rdx)                                                                                                               \
  V(rbx)                                                                                                               \
  V(rsp)                                                                                                               \
  V(rbp)                                                                                                               \
  V(rsi)                                                                                                               \
  V(rdi)                                                                                                               \
  V(r8)                                                                                                                \
  V(r9)                                                                                                                \
  V(r10)                                                                                                               \
  V(r11)                                                                                                               \
  V(r12)                                                                                                               \
  V(r13)                                                                                                               \
  V(r14)                                                                                                               \
  V(r15)

#define GENERAL_32_REGISTERS(V)                                                                                        \
  V(eax)                                                                                                               \
  V(ecx)                                                                                                               \
  V(edx)                                                                                                               \
  V(ebx)                                                                                                               \
  V(esp)                                                                                                               \
  V(ebp)                                                                                                               \
  V(esi)                                                                                                               \
  V(edi)

#define GENERAL_16_REGISTERS(V)                                                                                        \
  V(ax)                                                                                                                \
  V(cx)                                                                                                                \
  V(dx)                                                                                                                \
  V(bx)                                                                                                                \
  V(sp)                                                                                                                \
  V(bp)                                                                                                                \
  V(si)                                                                                                                \
  V(di)

#define GENERAL_8H_REGISTERS(V)                                                                                        \
  V(ah)                                                                                                                \
  V(ch)                                                                                                                \
  V(dh)                                                                                                                \
  V(bh)

#define GENERAL_8L_REGISTERS(V)                                                                                        \
  V(al)                                                                                                                \
  V(cl)                                                                                                                \
  V(dl)                                                                                                                \
  V(bl)

// clang-format off
enum RegisterCode {
#define REGISTER_CODE(R) kRegCode_##R,
  kRegisterCodeStart8L = -1,
  GENERAL_8L_REGISTERS(REGISTER_CODE)
  kRegisterCodeStart8H = -1,
  GENERAL_8H_REGISTERS(REGISTER_CODE)
  kRegisterCodeStart16 = -1,
  GENERAL_16_REGISTERS(REGISTER_CODE)
  kRegisterCodeStart32 = -1,
  GENERAL_32_REGISTERS(REGISTER_CODE)
  kRegisterCodeStart64 = -1,
  GENERAL_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
  kRegAfterLast
};
// clang-format on

class CPURegister : public RegisterBase {
public:
  enum RegisterType { kDefault, kInvalid };

  constexpr CPURegister(int code, int size, RegisterType type) : RegisterBase(code), reg_size_(size), reg_type_(type) {
  }

  static constexpr CPURegister Create(int code, int size, RegisterType type) {
    return CPURegister(code, size, type);
  }

  static constexpr CPURegister from_code(int code) {
    return CPURegister(code, 0, kDefault);
  }

  static constexpr CPURegister InvalidRegister() {
    return CPURegister(0, 0, kInvalid);
  }

  bool Is64Bits() const {
    return reg_size_ == 64;
  }

  RegisterType type() const {
    return reg_type_;
  }

public:
  bool is_byte_register() const {
    return reg_code_ <= 3;
  }

  // Return the high bit of the register code as a 0 or 1.  Used often
  // when constructing the REX prefix byte.
  int high_bit() const {
    return reg_code_ >> 3;
  }

  // Return the 3 low bits of the register code.  Used when encoding registers
  // in modR/M, SIB, and opcode bytes.
  int low_bits() const {
    return reg_code_ & 0x7;
  }

  int size() {
    return reg_size_;
  }

private:
  RegisterType reg_type_;
  int reg_size_;
};

typedef CPURegister Register;

#define DECLARE_REGISTER(R) constexpr Register R = Register::Create(kRegCode_##R, 64, CPURegister::kDefault);
GENERAL_REGISTERS(DECLARE_REGISTER)
#undef DECLARE_REGISTER

#define DECLARE_REGISTER(R) constexpr Register R = Register::Create(kRegCode_##R, 8, CPURegister::kDefault);
GENERAL_8H_REGISTERS(DECLARE_REGISTER)
#undef DECLARE_REGISTER

#define DECLARE_REGISTER(R) constexpr Register R = Register::Create(kRegCode_##R, 8, CPURegister::kDefault);
GENERAL_8L_REGISTERS(DECLARE_REGISTER)
#undef DECLARE_REGISTER

#define DECLARE_REGISTER(R) constexpr Register R = Register::Create(kRegCode_##R, 16, CPURegister::kDefault);
GENERAL_16_REGISTERS(DECLARE_REGISTER)
#undef DECLARE_REGISTER

#define DECLARE_REGISTER(R) constexpr Register R = Register::Create(kRegCode_##R, 32, CPURegister::kDefault);
GENERAL_32_REGISTERS(DECLARE_REGISTER)
#undef DECLARE_REGISTER

#ifdef _WIN64
// Windows calling convention
constexpr Register arg_reg_1 = rcx;
constexpr Register arg_reg_2 = rdx;
constexpr Register arg_reg_3 = r8;
constexpr Register arg_reg_4 = r9;
#else
// AMD64 calling convention
constexpr Register arg_reg_1 = rdi;
constexpr Register arg_reg_2 = rsi;
constexpr Register arg_reg_3 = rdx;
constexpr Register arg_reg_4 = rcx;
#endif // _WIN64

#define DOUBLE_REGISTERS(V)                                                                                            \
  V(xmm0)                                                                                                              \
  V(xmm1)                                                                                                              \
  V(xmm2)                                                                                                              \
  V(xmm3)                                                                                                              \
  V(xmm4)                                                                                                              \
  V(xmm5)                                                                                                              \
  V(xmm6)                                                                                                              \
  V(xmm7)                                                                                                              \
  V(xmm8)                                                                                                              \
  V(xmm9)                                                                                                              \
  V(xmm10)                                                                                                             \
  V(xmm11)                                                                                                             \
  V(xmm12)                                                                                                             \
  V(xmm13)                                                                                                             \
  V(xmm14)                                                                                                             \
  V(xmm15)

#define FLOAT_REGISTERS DOUBLE_REGISTERS
#define SIMD128_REGISTERS DOUBLE_REGISTERS

constexpr bool kPadArguments = false;
constexpr bool kSimpleFPAliasing = true;
constexpr bool kSimdMaskRegisters = false;

enum DoubleRegisterCode {
#define REGISTER_CODE(R) kDoubleCode_##R,
  DOUBLE_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kDoubleAfterLast
};

class XMMRegister : public RegisterBase {
public:
  enum RegisterType { kInvalid };

  constexpr XMMRegister(int code) : RegisterBase(code) {
  }

  static constexpr XMMRegister Create(int code) {
    return XMMRegister(code);
  }

  static constexpr XMMRegister InvalidRegister() {
    return XMMRegister(0);
  }

public:
  // Return the high bit of the register code as a 0 or 1.  Used often
  // when constructing the REX prefix byte.
  int high_bit() const {
    return reg_code_ >> 3;
  }
  // Return the 3 low bits of the register code.  Used when encoding registers
  // in modR/M, SIB, and opcode bytes.
  int low_bits() const {
    return reg_code_ & 0x7;
  }

private:
};

typedef XMMRegister FloatRegister;

typedef XMMRegister DoubleRegister;

typedef XMMRegister Simd128Register;

typedef XMMRegister FPURegister;

#define DECLARE_REGISTER(R) constexpr DoubleRegister R = DoubleRegister::Create(kDoubleCode_##R);
DOUBLE_REGISTERS(DECLARE_REGISTER)
#undef DECLARE_REGISTER

} // namespace x64
} // namespace zz

#endif
