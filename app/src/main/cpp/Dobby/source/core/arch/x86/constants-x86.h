#ifndef CORE_ARCH_CONSTANTS_X86_H
#define CORE_ARCH_CONSTANTS_X86_H

namespace zz {
namespace x86 {

enum ScaleFactor {
  TIMES_1 = 0,
  TIMES_2 = 1,
  TIMES_4 = 2,
  TIMES_8 = 3,
  TIMES_16 = 4,
  TIMES_HALF_WORD_SIZE = sizeof(void *) / 2 - 1
};

} // namespace x86
} // namespace zz

#endif