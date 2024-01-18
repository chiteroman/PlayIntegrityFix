#if defined(ENABLE_CLOSURE_BRIDGE_TEMPLATE)

#if defined(__WIN32__) || defined(__APPLE__)
#define cdecl(s) _##s
#else
#define cdecl(s) s
#endif

.align 4

; closure trampoline just carray the required members from the object.
.globl cdecl(closure_trampoline_template)
cdecl(closure_trampoline_template):
    push [rip+6+6]
    jmp [rip+6+8]
carry_data:
    .long 0
    .long 0
carry_handler:
    .long 0
    .long 0

#endif
