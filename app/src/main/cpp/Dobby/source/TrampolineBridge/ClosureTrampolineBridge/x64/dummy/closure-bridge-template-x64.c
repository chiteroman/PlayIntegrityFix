#if defined(ENABLE_CLOSURE_BRIDGE_TEMPLATE)

#if defined(__WIN32__) || defined(__APPLE__)
#define xcdecl(s) "_" s
#else
#define xcdecl(s) s
#endif

#define xASM(x) __asm(x)

__attribute__((naked)) void closure_bridge_template() {
  // flags register
  xASM("pushfq");

  // general register
  xASM("sub rsp, #(16*8)");
  xASM("mov [rsp+16*0], rax");
  xASM("mov [rsp+16*1], rbx");
  xASM("mov [rsp+16*2], rcx");
  xASM("mov [rsp+16*3], rdx");
  xASM("mov [rsp+16*4], rbp");
  xASM("mov [rsp+16*5], rsp");
  xASM("mov [rsp+16*6], rdi");
  xASM("mov [rsp+16*7], rsi");
  xASM("mov [rsp+16*8], r8");
  xASM("mov [rsp+16*9], r9");
  xASM("mov [rsp+16*10], r10");
  xASM("mov [rsp+16*11], r11");
  xASM("mov [rsp+16*12], r12");
  xASM("mov [rsp+16*13], r13");
  xASM("mov [rsp+16*14], r14");
  xASM("mov [rsp+16*15], r15");

  // ======= Jump to UnifiedInterface Bridge Handle =======

  // prepare args
  // @rdi: data_address
  // @rsi: DobbyRegisterContext stack address
  xASM("mov rdi, rsp");
  xASM("mov rsi, [rsp-16*8]");
  xASM("call " xcdecl("common_closure_bridge_handler"));

  // ======= DobbyRegisterContext Restore =======

  // general register
  xASM("pop r15");
  xASM("pop r14");
  xASM("pop r13");
  xASM("pop r12");
  xASM("pop r11");
  xASM("pop r10");
  xASM("pop r9");
  xASM("pop r8");
  xASM("pop rsi");
  xASM("pop rdi");
  xASM("pop rsp");
  xASM("pop rbp");
  xASM("pop rdx");
  xASM("pop rcx");
  xASM("pop rbx");
  xASM("pop rax");

  // flags register
  xASM("popfq");

  // trick: use the 'carry_data' placeholder, as the return address
  xASM("ret");
};

#endif