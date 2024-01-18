#pragma once

#if !defined(DISABLE_ARCH_DETECT)
#if defined(__arm__)
#define TARGET_ARCH_ARM 1
#elif defined(__arm64__) || defined(__aarch64__)
#define TARGET_ARCH_ARM64 1
#elif defined(_M_IX86) || defined(__i386__)
#define TARGET_ARCH_IA32 1
#elif defined(_M_X64) || defined(__x86_64__)
#define TARGET_ARCH_X64 1
#else
#error Target architecture was not detected as supported by Dobby
#endif
#endif
