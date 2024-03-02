#ifndef CORE_ASSEMBLER_ARCH_H
#define CORE_ASSEMBLER_ARCH_H

#include "src/assembler.h"

#if 0
#if TARGET_ARCH_IA32
#include "src/ia32/assembler-ia32.h"
#elif TARGET_ARCH_X64
#include "src/x64/assembler-x64.h"
#elif TARGET_ARCH_ARM64
#include "src/arm64/assembler-arm64.h"
#elif TARGET_ARCH_ARM
#include "src/arm/assembler-arm.h"
#elif TARGET_ARCH_PPC
#include "src/ppc/assembler-ppc.h"
#elif TARGET_ARCH_MIPS
#include "src/mips/assembler-mips.h"
#elif TARGET_ARCH_MIPS64
#include "src/mips64/assembler-mips64.h"
#elif TARGET_ARCH_S390
#include "src/s390/assembler-s390.h"
#else
#error Unknown architecture.
#endif
#endif

#endif
