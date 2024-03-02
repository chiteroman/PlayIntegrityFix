#ifndef ARCH_ARM64_REGISTERS
#define ARCH_ARM64_REGISTERS

#include "core/arch/arm64/constants-arm64.h"
#include "core/arch/Cpu.h"

namespace zz {
namespace arm64 {

class CPURegister : RegisterBase {
public:
  enum RegisterType {
    kRegister_32,
    kRegister_W = kRegister_32,
    kRegister_64,
    kRegister_X = kRegister_64,
    kRegister,

    kVRegister,
    kSIMD_FP_Register_8,
    kSIMD_FP_Register_B = kSIMD_FP_Register_8,
    kSIMD_FP_Register_16,
    kSIMD_FP_Register_H = kSIMD_FP_Register_16,
    kSIMD_FP_Register_32,
    kSIMD_FP_Register_S = kSIMD_FP_Register_32,
    kSIMD_FP_Register_64,
    kSIMD_FP_Register_D = kSIMD_FP_Register_64,
    kSIMD_FP_Register_128,
    kSIMD_FP_Register_Q = kSIMD_FP_Register_128,

    kInvalid
  };

  constexpr CPURegister(int code, int size, RegisterType type) : RegisterBase(code), reg_size_(size), reg_type_(type) {
  }

  static constexpr CPURegister Create(int code, int size, RegisterType type) {
    return CPURegister(code, size, type);
  }

  // =====

  static constexpr CPURegister X(int code) {
    return CPURegister(code, 64, kRegister_64);
  }

  static constexpr CPURegister W(int code) {
    return CPURegister(code, 32, kRegister_32);
  }

  static constexpr CPURegister Q(int code) {
    return CPURegister(code, 128, kSIMD_FP_Register_128);
  }

  static constexpr CPURegister InvalidRegister() {
    return CPURegister(0, 0, kInvalid);
  }

  // =====

  bool Is(const CPURegister &reg) const {
    return (reg.reg_code_ == this->reg_code_);
  }

  bool Is64Bits() const {
    return reg_size_ == 64;
  }

  bool IsRegister() const {
    return reg_type_ < kRegister;
  }

  bool IsVRegister() const {
    return reg_type_ > kVRegister;
  }

  // =====

  RegisterType type() const {
    return reg_type_;
  }

  int32_t code() const {
    return reg_code_;
  };

private:
  RegisterType reg_type_;
  int reg_size_;
};

typedef CPURegister Register;
typedef CPURegister VRegister;

// clang-format off
#define GENERAL_REGISTER_CODE_LIST(R)                     \
  R(0)  R(1)  R(2)  R(3)  R(4)  R(5)  R(6)  R(7)          \
  R(8)  R(9)  R(10) R(11) R(12) R(13) R(14) R(15)         \
  R(16) R(17) R(18) R(19) R(20) R(21) R(22) R(23)         \
  R(24) R(25) R(26) R(27) R(28) R(29) R(30) R(31)

#define DEFINE_REGISTER(register_class, name, ...) constexpr register_class name = register_class::Create(__VA_ARGS__)

#define DEFINE_REGISTERS(N)                                                                                            \
  DEFINE_REGISTER(Register, w##N, N, 32, CPURegister::kRegister_32);                                                                 \
  DEFINE_REGISTER(Register, x##N, N, 64, CPURegister::kRegister_64);
    GENERAL_REGISTER_CODE_LIST(DEFINE_REGISTERS)
#undef DEFINE_REGISTERS

#define DEFINE_VREGISTERS(N)  \
  DEFINE_REGISTER(VRegister, b##N, N, 8, CPURegister::kSIMD_FP_Register_8);                                                                \
  DEFINE_REGISTER(VRegister, h##N, N, 16, CPURegister::kSIMD_FP_Register_16);                                                                \
  DEFINE_REGISTER(VRegister, s##N, N, 32, CPURegister::kSIMD_FP_Register_32);                                                                \
  DEFINE_REGISTER(VRegister, d##N, N, 64, CPURegister::kSIMD_FP_Register_64);                                                                \
  DEFINE_REGISTER(VRegister, q##N, N, 128, CPURegister::kSIMD_FP_Register_128);                                                                \
GENERAL_REGISTER_CODE_LIST(DEFINE_VREGISTERS)
#undef DEFINE_VREGISTERS

#undef DEFINE_REGISTER
// clang-format on

// =====

constexpr Register wzr = w31;
constexpr Register xzr = x31;

constexpr Register SP = x31;
constexpr Register wSP = w31;
constexpr Register FP = x29;
constexpr Register wFP = w29;
constexpr Register LR = x30;
constexpr Register wLR = w30;

} // namespace arm64
} // namespace zz

#define W(code) CPURegister::W(code)
#define X(code) CPURegister::X(code)
#define Q(code) CPURegister::Q(code)
#define InvalidRegister CPURegister::InvalidRegister()

#endif
