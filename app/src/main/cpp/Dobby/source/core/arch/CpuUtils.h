#ifndef CPU_UTILITY_H
#define CPU_UTILITY_H

/* Define the default attributes for the functions in this file. */
#define __DEFAULT_FN_ATTRS __attribute__((__always_inline__, __nodebug__))

#if defined(__i386__) || defined(__x86_64__)
static __inline__ void __DEFAULT_FN_ATTRS __cpuid(int __info[4], int __level) {
  __asm__("cpuid" : "=a"(__info[0]), "=b"(__info[1]), "=c"(__info[2]), "=d"(__info[3]) : "a"(__level));
}

static __inline__ void __DEFAULT_FN_ATTRS __cpuidex(int __info[4], int __level, int __ecx) {
  __asm__("cpuid" : "=a"(__info[0]), "=b"(__info[1]), "=c"(__info[2]), "=d"(__info[3]) : "a"(__level), "c"(__ecx));
}
#endif

#endif
