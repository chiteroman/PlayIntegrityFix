// .section	__TEXT,__text,regular,pure_instructions
// .ios_version_min 11, 0

#if defined(ENABLE_CLOSURE_BRIDGE_TEMPLATE)

#if defined(__WIN32__) || defined(__APPLE__)
#define cdecl(s) "_" s
#else
#define cdecl(s) s
#endif

#define xASM(x) __asm(x)

__attribute__((naked)) void closure_bridge_template() {
  xASM(".arm");
  xASM("sub sp, sp, #(14*4)");
  xASM("str lr, [sp, #(13*4)]");
  xASM("str r12, [sp, #(12*4)]");
  xASM("str r11, [sp, #(11*4)]");
  xASM("str r10, [sp, #(10*4)]");
  xASM("str r9, [sp, #(9*4)]");
  xASM("str r8, [sp, #(8*4)]");
  xASM("str r7, [sp, #(7*4)]");
  xASM("str r6, [sp, #(6*4)]");
  xASM("str r5, [sp, #(5*4)]");
  xASM("str r4, [sp, #(4*4)]");
  xASM("str r3, [sp, #(3*4)]");
  xASM("str r2, [sp, #(2*4)]");
  xASM("str r1, [sp, #(1*4)]");
  xASM("str r0, [sp, #(0*4)]");

  // dummy align
  xASM("sub sp, sp, #8");

  xASM("mov r0, sp");
  xASM("mov r1, r12");
  xASM("bl " cdecl("common_closure_bridge_handler"));

  // dummy align
  xASM("add sp, sp, #8");

  xASM("ldr r0, [sp], #4");
  xASM("ldr r1, [sp], #4");
  xASM("ldr r2, [sp], #4");
  xASM("ldr r3, [sp], #4");
  xASM("ldr r4, [sp], #4");
  xASM("ldr r5, [sp], #4");
  xASM("ldr r6, [sp], #4");
  xASM("ldr r7, [sp], #4");
  xASM("ldr r8, [sp], #4");
  xASM("ldr r9, [sp], #4");
  xASM("ldr r10, [sp], #4");
  xASM("ldr r11, [sp], #4");
  xASM("ldr r12, [sp], #4");
  xASM("ldr lr, [sp], #4");

#if 1
  xASM("str r12, [sp, #-4]");
  xASM("ldr pc, [sp, #-4]");
#else
  xASM("mov pc, r12");
#endif
}

#endif