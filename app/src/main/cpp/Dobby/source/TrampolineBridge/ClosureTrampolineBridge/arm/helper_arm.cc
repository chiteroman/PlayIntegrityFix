#include "platform_macro.h"
#if defined(TARGET_ARCH_ARM)

#include "dobby_internal.h"

void set_routing_bridge_next_hop(DobbyRegisterContext *ctx, void *address) {
  *reinterpret_cast<void **>(&ctx->general.regs.r12) = address;
}

void get_routing_bridge_next_hop(DobbyRegisterContext *ctx, void *address) {
}

#endif