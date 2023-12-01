#ifndef CLOSURE_TRAMPOLINE_COMMON_HANDLER_H
#define CLOSURE_TRAMPOLINE_COMMON_HANDLER_H

#include "dobby/dobby_internal.h"

#include "Interceptor.h"
#include "TrampolineBridge/ClosureTrampolineBridge/ClosureTrampoline.h"

extern "C" {
void common_closure_bridge_handler(DobbyRegisterContext *ctx, ClosureTrampolineEntry *entry);
}

void get_routing_bridge_next_hop(DobbyRegisterContext *ctx, void *address);

void set_routing_bridge_next_hop(DobbyRegisterContext *ctx, void *address);

#endif