#if defined(ENABLE_CLOSURE_BRIDGE_TEMPLATE)

#if defined(__WIN32__) || defined(__APPLE__)
#define xcdecl(s) "_" s
#else
#define xcdecl(s) s
#endif

#define xASM(x) __asm(x)

__attribute__((naked)) void closure_bridge_template() {
  // DO NOT USE prologue
  // x29 == fp, x30 == lr
  // xASM("stp x29, x30, [sp, #-16]!");
  // xASM("mov x29, sp");

  // save {q0-q7}
  xASM("sub sp, sp, #(8*16)");
  xASM("stp q6, q7, [sp, #(6*16)]");
  xASM("stp q4, q5, [sp, #(4*16)]");
  xASM("stp q2, q3, [sp, #(2*16)]");
  xASM("stp q0, q1, [sp, #(0*16)]");

  // save {x1-x30}
  xASM("sub sp, sp, #(30*8)");
  // stp fp, lr, [sp, #(28*8)]");
  xASM("stp x29, x30, [sp, #(28*8)]");
  xASM("stp x27, x28, [sp, #(26*8)]");
  xASM("stp x25, x26, [sp, #(24*8)]");
  xASM("stp x23, x24, [sp, #(22*8)]");
  xASM("stp x21, x22, [sp, #(20*8)]");
  xASM("stp x19, x20, [sp, #(18*8)]");
  xASM("stp x17, x18, [sp, #(16*8)]");
  xASM("stp x15, x16, [sp, #(14*8)]");
  xASM("stp x13, x14, [sp, #(12*8)]");
  xASM("stp x11, x12, [sp, #(10*8)]");
  xASM("stp x9, x10, [sp, #(8*8)]");
  xASM("stp x7, x8, [sp, #(6*8)]");
  xASM("stp x5, x6, [sp, #(4*8)]");
  xASM("stp x3, x4, [sp, #(2*8)]");
  xASM("stp x1, x2, [sp, #(0*8)]");

#if 1
  // save {x0}
  xASM("sub sp, sp, #(2*8)");
  xASM("str x0, [sp, #8]");
#else
  // save {x0, sp}
  // save x0 and reserve sp, but this is trick
  xASM("sub sp, sp, #(2*8)");
  xASM("str x0, [sp, #8]");
  // save origin sp
  xASM("add x1, sp, #0x190");
  xASM("str x1, [sp, #0]");
#endif

  // ======= Jump to UnifiedInterface Bridge Handle =======

  // prepare args
  // @x0: data_address
  // @x1: DobbyRegisterContext stack address
  xASM("mov x0, sp");
  xASM("mov x1, x14");
  xASM("bl " xcdecl("common_closure_bridge_handler"));

  // ======= DobbyRegisterContext Restore =======
  // restore x0
  xASM("ldr x0, [sp, #8]");
  xASM("add sp, sp, #(2*8)");

  // restore {x1-x30}
  xASM("ldp x1, x2, [sp], #16");
  xASM("ldp x3, x4, [sp], #16");
  xASM("ldp x5, x6, [sp], #16");
  xASM("ldp x7, x8, [sp], #16");
  xASM("ldp x9, x10, [sp], #16");
  xASM("ldp x11, x12, [sp], #16");
  xASM("ldp x13, x14, [sp], #16");
  xASM("ldp x15, x16, [sp], #16");
  xASM("ldp x17, x18, [sp], #16");
  xASM("ldp x19, x20, [sp], #16");
  xASM("ldp x21, x22, [sp], #16");
  xASM("ldp x23, x24, [sp], #16");
  xASM("ldp x25, x26, [sp], #16");
  xASM("ldp x27, x28, [sp], #16");
  // ldp fp, lr, [sp], #16");
  xASM("ldp x29, x30, [sp], #16");

  // restore {q0-q7}
  xASM("ldp q0, q1, [sp], #32");
  xASM("ldp q2, q3, [sp], #32");
  xASM("ldp q4, q5, [sp], #32");
  xASM("ldp q6, q7, [sp], #32");

  // DO NOT USE epilog
  // x29 == fp, x30 == lr
  // xASM("mov sp, x29");
  // xASM("ldp x29, x30, [sp], #16");

  xASM("br x15");
};

#endif