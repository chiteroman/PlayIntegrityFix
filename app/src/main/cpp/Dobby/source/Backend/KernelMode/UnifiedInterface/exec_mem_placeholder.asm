#include <mach/machine/asm.h>

#define PAGE_SHIFT 14
.align PAGE_SHIFT

  .globl EXT(kernel_executable_memory_placeholder)
EXT(kernel_executable_memory_placeholder):
.rept 0x4000/4
.long 0x41414141
.endr