#ifndef USER_MODE_CLEAR_CACHE_TOOL_H
#define USER_MODE_CLEAR_CACHE_TOOL_H

#include "core/arch/Cpu.h"

#include "PlatformInterface/globals.h"

#if !HOST_OS_IOS
#include <sys/syscall.h> // for cache flushing.
#endif

void CpuFeatures::FlushICache(void *startp, void *endp) {

#if HOST_OS_IOS
  // Precompilation never patches code so there should be no I cache flushes.
  CpuFeatures::ClearCache(startp, endp);

#else

  register uint32_t beg asm("r0") = reinterpret_cast<uint32_t>(startp);
  register uint32_t end asm("r1") = reinterpret_cast<uint32_t>(endp);
  register uint32_t flg asm("r2") = 0;

#ifdef __clang__
  // This variant of the asm avoids a constant pool entry, which can be
  // problematic when LTO'ing. It is also slightly shorter.
  register uint32_t scno asm("r7") = __ARM_NR_cacheflush;

  asm volatile("svc 0\n" : : "r"(beg), "r"(end), "r"(flg), "r"(scno) : "memory");
#else
  // Use a different variant of the asm with GCC because some versions doesn't
  // support r7 as an asm input.
  asm volatile(
      // This assembly works for both ARM and Thumb targets.

      // Preserve r7; it is callee-saved, and GCC uses it as a frame pointer for
      // Thumb targets.
      "  push {r7}\n"
      // r0 = beg
      // r1 = end
      // r2 = flags (0)
      "  ldr r7, =%c[scno]\n" // r7 = syscall number
      "  svc 0\n"

      "  pop {r7}\n"
      :
      : "r"(beg), "r"(end), "r"(flg), [scno] "i"(__ARM_NR_cacheflush)
      : "memory");
#endif
#endif
}

#endif