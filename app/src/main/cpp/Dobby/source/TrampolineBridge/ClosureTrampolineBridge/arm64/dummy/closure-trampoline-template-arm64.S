// .section	__TEXT,__text,regular,pure_instructions

#if defined(ENABLE_CLOSURE_BRIDGE_TEMPLATE)

#if defined(__WIN32__) || defined(__APPLE__)
#define cdecl(s) _##s
#else
#define cdecl(s) s
#endif

.align 4

#if !defined(ENABLE_CLOSURE_TRAMPOLINE_CARRY_OBJECT_PTR)

// closure trampoline carray the object pointer, and fetch required members at the runtime assembly code.
// #include "TrampolineBridge/ClosureTrampolineBridge/ClosureTrampoline.h"
// #define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
#define OFFSETOF_ClourseTrampolineEntry_carry_data 8
#define OFFSETOF_ClourseTrampolineEntry_carry_handler 0
    .globl
    cdecl(closure_trampoline_template) cdecl(closure_trampoline_template)
    : ldr x17,
ClourseTrampolineEntryPtr ldr x16, OFFSETOF_ClourseTrampolineEntry_carry_data ldr x17,
OFFSETOF_ClourseTrampolineEntry_carry_handler br x17 ClourseTrampolineEntryPtr :.long 0.long 0

#else

    ;
closure trampoline just carray the required members from the object..globl cdecl(closure_trampoline_template)
    cdecl(closure_trampoline_template)
    : ldr x16,
= carry_data ldr x17, = carry_handler br x17 carry_data :.long 0.long 0 carry_handler :.long 0.long 0

#endif

#endif