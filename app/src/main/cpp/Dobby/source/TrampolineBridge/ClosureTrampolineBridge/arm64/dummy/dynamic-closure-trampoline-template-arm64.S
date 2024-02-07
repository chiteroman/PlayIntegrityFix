// .section	__TEXT,__text,regular,pure_instructions

// For iOS, we can't allocate executable memory, but we can use `remap` doing some trick.
// For details, please refer `libffi`

#if defined(ENABLE_CLOSURE_BRIDGE_TEMPLATE)

#if defined(__WIN32__) || defined(__APPLE__)
#define cdecl(s) _##s
#else
#define cdecl(s) s
#endif

#define PAGE_MAX_SIZE 4096
#define PAGE_MAX_SHIFT 14

.align PAGE_MAX_SHIFT.globl cdecl(dynamic_closure_trampoline_table_page) cdecl(dynamic_closure_trampoline_table_page)
    :.rept(PAGE_MAX_SIZE - 4 * 4) /
    8 // sub dynamic_closure_trampoline_forward size
    adr x16,
#0 b cdecl(dynamic_closure_trampoline_forward)
    .endr

    cdecl(dynamic_closure_trampoline_forward)
    : sub x16,
x16, #0x4000 // [DynamicClosureTrampoline **]
    ldr x16,
[ x16, #0 ] // [DynamicClosureTrampoline *]
    ldr x17,
[ x16, #0 ] // trampolineTo
    br x17

#endif