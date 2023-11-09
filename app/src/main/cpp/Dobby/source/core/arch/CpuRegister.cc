
#include "CpuRegister.h"

constexpr RegisterBase RegisterBase::from_code(int code) {
  return RegisterBase{code};
}

constexpr RegisterBase RegisterBase::no_reg() {
  return RegisterBase{0};
}