#include "platform_macro.h"
#if defined(TARGET_ARCH_ARM64)

#include "dobby_internal.h"

#include "core/assembler/assembler.h"
#include "core/assembler/assembler-arm64.h"

#include "TrampolineBridge/ClosureTrampolineBridge/common_bridge_handler.h"

using namespace zz;
using namespace zz::arm64;

static asm_func_t closure_bridge = nullptr;

asm_func_t get_closure_bridge() {
  // if already initialized, just return.
  if (closure_bridge)
    return closure_bridge;

// check if enable the inline-assembly closure_bridge_template
#if ENABLE_CLOSURE_BRIDGE_TEMPLATE
  extern void closure_bridge_tempate();
  closure_bridge = closure_bridge_template;
// otherwise, use the Assembler build the closure_bridge
#else
#define _ turbo_assembler_.
#define MEM(reg, offset) MemOperand(reg, offset)
  TurboAssembler turbo_assembler_(0);

#if defined(FULL_FLOATING_POINT_REGISTER_PACK)

  _ sub(SP, SP, 24 * 16);
  _ stp(Q(30), Q(31), MEM(SP, 22 * 16));
  _ stp(Q(28), Q(29), MEM(SP, 20 * 16));
  _ stp(Q(26), Q(27), MEM(SP, 18 * 16));
  _ stp(Q(24), Q(25), MEM(SP, 16 * 16));
  _ stp(Q(22), Q(23), MEM(SP, 14 * 16));
  _ stp(Q(20), Q(21), MEM(SP, 12 * 16));
  _ stp(Q(18), Q(19), MEM(SP, 10 * 16));
  _ stp(Q(16), Q(17), MEM(SP, 8 * 16));
  _ stp(Q(14), Q(15), MEM(SP, 6 * 16));
  _ stp(Q(12), Q(13), MEM(SP, 4 * 16));
  _ stp(Q(10), Q(11), MEM(SP, 2 * 16));
  _ stp(Q(8), Q(9), MEM(SP, 0 * 16));

#endif

  // save {q0-q7}
  _ sub(SP, SP, 8 * 16);
  _ stp(Q(6), Q(7), MEM(SP, 6 * 16));
  _ stp(Q(4), Q(5), MEM(SP, 4 * 16));
  _ stp(Q(2), Q(3), MEM(SP, 2 * 16));
  _ stp(Q(0), Q(1), MEM(SP, 0 * 16));

  // save {x1-x30}
  _ sub(SP, SP, 30 * 8);
  _ stp(X(29), X(30), MEM(SP, 28 * 8));
  _ stp(X(27), X(28), MEM(SP, 26 * 8));
  _ stp(X(25), X(26), MEM(SP, 24 * 8));
  _ stp(X(23), X(24), MEM(SP, 22 * 8));
  _ stp(X(21), X(22), MEM(SP, 20 * 8));
  _ stp(X(19), X(20), MEM(SP, 18 * 8));
  _ stp(X(17), X(18), MEM(SP, 16 * 8));
  _ stp(X(15), X(16), MEM(SP, 14 * 8));
  _ stp(X(13), X(14), MEM(SP, 12 * 8));
  _ stp(X(11), X(12), MEM(SP, 10 * 8));
  _ stp(X(9), X(10), MEM(SP, 8 * 8));
  _ stp(X(7), X(8), MEM(SP, 6 * 8));
  _ stp(X(5), X(6), MEM(SP, 4 * 8));
  _ stp(X(3), X(4), MEM(SP, 2 * 8));
  _ stp(X(1), X(2), MEM(SP, 0 * 8));

  // save {x0}
  _ sub(SP, SP, 2 * 8);
  _ str(x0, MEM(SP, 8));

  // calculate original sp
  _ add(TMP_REG_0, SP, 2 * 8);                          // closure trampoline reserved
  _ add(TMP_REG_0, TMP_REG_0, 2 * 8 + 30 * 8 + 8 * 16); // x0, x1-x30, q0-q7 reserved
#if defined(FULL_FLOATING_POINT_REGISTER_PACK)
  _ add(TMP_REG_0, TMP_REG_0, 24 * 16);                 // q8-q31 reserved
#endif

  // alloc stack, store original sp
  _ sub(SP, SP, 2 * 8);
  _ str(TMP_REG_0, MEM(SP, 8));

#if defined(FULL_FLOATING_POINT_REGISTER_PACK)
#define REGISTER_CONTEXT_SIZE (sizeof(DobbyRegisterContext))
#else
#define REGISTER_CONTEXT_SIZE (sizeof(DobbyRegisterContext) - 24 * 16)
#endif
  // create function arm64 call convention
  _ mov(x0, SP); // arg1: register context
  // load package(closure trampoline entry reserved)
  _ ldr(x1, MEM(SP, REGISTER_CONTEXT_SIZE + 0)); // arg2: closure trampoline entry
  _ CallFunction(ExternalReference((void *)common_closure_bridge_handler));

  // restore sp placeholder stack
  _ add(SP, SP, 2 * 8);

  // restore {x0}
  _ ldr(X(0), MEM(SP, 8));
  _ add(SP, SP, 2 * 8);

#define MEM_EXT(reg, offset, addrmode) MemOperand(reg, offset, addrmode)
  // restore {x1-x30}
  _ ldp(X(1), X(2), MEM_EXT(SP, 16, PostIndex));
  _ ldp(X(3), X(4), MEM_EXT(SP, 16, PostIndex));
  _ ldp(X(5), X(6), MEM_EXT(SP, 16, PostIndex));
  _ ldp(X(7), X(8), MEM_EXT(SP, 16, PostIndex));
  _ ldp(X(9), X(10), MEM_EXT(SP, 16, PostIndex));
  _ ldp(X(11), X(12), MEM_EXT(SP, 16, PostIndex));
  _ ldp(X(13), X(14), MEM_EXT(SP, 16, PostIndex));
  _ ldp(X(15), X(16), MEM_EXT(SP, 16, PostIndex));
  _ ldp(X(17), X(18), MEM_EXT(SP, 16, PostIndex));
  _ ldp(X(19), X(20), MEM_EXT(SP, 16, PostIndex));
  _ ldp(X(21), X(22), MEM_EXT(SP, 16, PostIndex));
  _ ldp(X(23), X(24), MEM_EXT(SP, 16, PostIndex));
  _ ldp(X(25), X(26), MEM_EXT(SP, 16, PostIndex));
  _ ldp(X(27), X(28), MEM_EXT(SP, 16, PostIndex));
  _ ldp(X(29), X(30), MEM_EXT(SP, 16, PostIndex));

  // restore {q0-q7}
  _ ldp(Q(0), Q(1), MEM_EXT(SP, 32, PostIndex));
  _ ldp(Q(2), Q(3), MEM_EXT(SP, 32, PostIndex));
  _ ldp(Q(4), Q(5), MEM_EXT(SP, 32, PostIndex));
  _ ldp(Q(6), Q(7), MEM_EXT(SP, 32, PostIndex));

#if defined(FULL_FLOATING_POINT_REGISTER_PACK)
  _ ldp(Q(8), Q(9), MEM_EXT(SP, 32, PostIndex));
  _ ldp(Q(10), Q(11), MEM_EXT(SP, 32, PostIndex));
  _ ldp(Q(12), Q(13), MEM_EXT(SP, 32, PostIndex));
  _ ldp(Q(14), Q(15), MEM_EXT(SP, 32, PostIndex));
  _ ldp(Q(16), Q(17), MEM_EXT(SP, 32, PostIndex));
  _ ldp(Q(18), Q(19), MEM_EXT(SP, 32, PostIndex));
  _ ldp(Q(20), Q(21), MEM_EXT(SP, 32, PostIndex));
  _ ldp(Q(22), Q(23), MEM_EXT(SP, 32, PostIndex));
  _ ldp(Q(24), Q(25), MEM_EXT(SP, 32, PostIndex));
  _ ldp(Q(26), Q(27), MEM_EXT(SP, 32, PostIndex));
  _ ldp(Q(28), Q(29), MEM_EXT(SP, 32, PostIndex));
  _ ldp(Q(30), Q(31), MEM_EXT(SP, 32, PostIndex));
#endif

  // _ brk(0); // for debug

  // return to closure trampoline, but TMP_REG_0, had been modified with next hop address
  _ ret(); // AKA br x30

  auto code = AssemblyCodeBuilder::FinalizeFromTurboAssembler(&turbo_assembler_);
  closure_bridge = (asm_func_t)code->addr;

  DLOG(0, "[closure bridge] closure bridge at %p", closure_bridge);
#endif
  return closure_bridge;
}

#endif
