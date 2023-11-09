#ifndef ARCH_IA32_REGISTERS
#define ARCH_IA32_REGISTERS

#include "core/arch/x86/constants-x86.h"
#include "core/arch/Cpu.h"

namespace zz {
namespace x86 {

#define GENERAL_REGISTERS(V)                                                                                           \
  V(eax)                                                                                                               \
  V(ecx)                                                                                                               \
  V(edx)                                                                                                               \
  V(ebx)                                                                                                               \
  V(esp)                                                                                                               \
  V(ebp)                                                                                                               \
  V(esi)                                                                                                               \
  V(edi)

enum RegisterCode {
#define REGISTER_CODE(R) kRegCode_##R,
  GENERAL_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kRegAfterLast
};

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

  RegisterType type() const {
    return reg_type_;
  }

public:
  bool is_byte_register() const {
    return reg_code_ <= 3;
  }

  int size() {
    return reg_size_;
  }

private:
  RegisterType reg_type_;
  int reg_size_;
};

typedef CPURegister Register;

#define DEFINE_REGISTER(R) constexpr Register R = Register::Create(kRegCode_##R, 32, CPURegister::kDefault);
GENERAL_REGISTERS(DEFINE_REGISTER)
#undef DEFINE_REGISTER

#define DOUBLE_REGISTERS(V)                                                                                            \
  V(xmm0)                                                                                                              \
  V(xmm1)                                                                                                              \
  V(xmm2)                                                                                                              \
  V(xmm3)                                                                                                              \
  V(xmm4)                                                                                                              \
  V(xmm5)                                                                                                              \
  V(xmm6)                                                                                                              \
  V(xmm7)

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

private:
};

typedef XMMRegister FloatRegister;
typedef XMMRegister DoubleRegister;
typedef XMMRegister Simd128Register;
typedef XMMRegister FPURegister;

#define DEFINE_REGISTER(R) constexpr DoubleRegister R = DoubleRegister::Create(kDoubleCode_##R);
DOUBLE_REGISTERS(DEFINE_REGISTER)
#undef DEFINE_REGISTER

} // namespace x86
} // namespace zz

#endif
