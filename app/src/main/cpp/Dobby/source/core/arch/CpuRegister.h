#ifndef CORE_ARCH_CPU_REGISTER_H
#define CORE_ARCH_CPU_REGISTER_H

class RegisterBase {
public:
  static constexpr RegisterBase from_code(int code);

  static constexpr RegisterBase no_reg();

  virtual bool Is(const RegisterBase &reg) const {
    return (reg.reg_code_ == this->reg_code_);
  }

  int code() const {
    return reg_code_;
  };

protected:
  explicit constexpr RegisterBase(int code) : reg_code_(code) {
  }

  int reg_code_;
};

#endif
