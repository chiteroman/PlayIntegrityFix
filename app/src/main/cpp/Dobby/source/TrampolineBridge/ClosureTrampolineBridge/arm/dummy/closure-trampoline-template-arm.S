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
#define OFFSETOF_ClourseTrampolineEntry_carry_data 4
#define OFFSETOF_ClourseTrampolineEntry_carry_handler 0
.globl cdecl(closure_trampoline_template)
cdecl(closure_trampoline_template):
    ldr r12, ClourseTrampolineEntryPtr
    ldr pc, [r12, #0]
ClourseTrampolineEntryPtr:
    .long 0

#else

; closure trampoline just carray the required members from the object.
.globl cdecl(closure_trampoline_template)
cdecl(closure_trampoline_template):
    ldr r12, =carry_data
    ldr pc, =carry_handler
carry_data:
    .long 0
carry_handler:
    .long 0
#endif

#endif