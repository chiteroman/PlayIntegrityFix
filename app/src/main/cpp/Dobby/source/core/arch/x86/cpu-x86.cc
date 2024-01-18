#include "platform_macro.h"
#if defined(TARGET_ARCH_IA32) || defined(TARGET_ARCH_X64)

#include "cpu-x86.h"

X86CpuInfo::X86CpuInfo() {
  icache_line_size_ = 0;
  dcache_line_size_ = 0;
  has_fpu_ = false;
  has_cmov_ = false;
  has_sahf_ = false;
  has_mmx_ = false;
  has_sse_ = false;
  has_sse2_ = false;
  has_sse3_ = false;
  has_ssse3_ = false;
  has_sse41_ = false;

  has_sse42_ = false;
  has_osxsave_ = false;
  has_avx_ = false;
  has_fma3_ = false;
  has_bmi1_ = false;
  has_bmi2_ = false;
  has_lzcnt_ = false;
  has_popcnt_ = false;
  is_atom_ = false;

  memcpy(vendor_, (void *)"Unknown", 8);
#if V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64
  int cpu_info[4];
  __cpuid(cpu_info, 0);
  unsigned num_ids = cpu_info[0];
  std::swap(cpu_info[2], cpu_info[3]);
  _memcpy(vendor_, cpu_info + 1, 12);
  vendor_[12] = '\0';

  // Interpret CPU feature information.
  if (num_ids > 0) {
    __cpuid(cpu_info, 1);
    stepping_ = cpu_info[0] & 0xF;
    model_ = ((cpu_info[0] >> 4) & 0xF) + ((cpu_info[0] >> 12) & 0xF0);
    family_ = (cpu_info[0] >> 8) & 0xF;
    type_ = (cpu_info[0] >> 12) & 0x3;
    ext_model_ = (cpu_info[0] >> 16) & 0xF;
    ext_family_ = (cpu_info[0] >> 20) & 0xFF;
    has_fpu_ = (cpu_info[3] & 0x00000001) != 0;
    has_cmov_ = (cpu_info[3] & 0x00008000) != 0;
    has_mmx_ = (cpu_info[3] & 0x00800000) != 0;
    has_sse_ = (cpu_info[3] & 0x02000000) != 0;
    has_sse2_ = (cpu_info[3] & 0x04000000) != 0;
    has_sse3_ = (cpu_info[2] & 0x00000001) != 0;
    has_ssse3_ = (cpu_info[2] & 0x00000200) != 0;
    has_sse41_ = (cpu_info[2] & 0x00080000) != 0;
    has_sse42_ = (cpu_info[2] & 0x00100000) != 0;
    has_popcnt_ = (cpu_info[2] & 0x00800000) != 0;
    has_osxsave_ = (cpu_info[2] & 0x08000000) != 0;
    has_avx_ = (cpu_info[2] & 0x10000000) != 0;
    has_fma3_ = (cpu_info[2] & 0x00001000) != 0;
    if (family_ == 0x6) {
      switch (model_) {
      case 0x1C: // SLT
      case 0x26:
      case 0x36:
      case 0x27:
      case 0x35:
      case 0x37: // SLM
      case 0x4A:
      case 0x4D:
      case 0x4C: // AMT
      case 0x6E:
        is_atom_ = true;
      }
    }
  }

  // There are separate feature flags for VEX-encoded GPR instructions.
  if (num_ids >= 7) {
    __cpuid(cpu_info, 7);
    has_bmi1_ = (cpu_info[1] & 0x00000008) != 0;
    has_bmi2_ = (cpu_info[1] & 0x00000100) != 0;
  }

  // Query extended IDs.
  __cpuid(cpu_info, 0x80000000);
  unsigned num_ext_ids = cpu_info[0];

  // Interpret extended CPU feature information.
  if (num_ext_ids > 0x80000000) {
    __cpuid(cpu_info, 0x80000001);
    has_lzcnt_ = (cpu_info[2] & 0x00000020) != 0;
    // SAHF must be probed in long mode.
    has_sahf_ = (cpu_info[2] & 0x00000001) != 0;
  }
#endif
}

#endif