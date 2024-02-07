#include "platform_detect_macro.h"
#if defined(TARGET_ARCH_X64)

#include "dobby/dobby_internal.h"

void set_routing_bridge_next_hop(DobbyRegisterContext *ctx, void *address) {
  addr_t rsp = ctx->rsp;

  // ClosureTrampolineEntry reserved stack
  addr_t entry_placeholder_stack_addr = rsp - 8;
  *(addr_t *)entry_placeholder_stack_addr = (addr_t)address;
}

void get_routing_bridge_next_hop(DobbyRegisterContext *ctx, void *address) {
}

#endif