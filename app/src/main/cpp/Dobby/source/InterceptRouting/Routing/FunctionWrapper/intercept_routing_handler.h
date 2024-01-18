#ifndef FUNCTION_WRAPPER_INTERCEPT_ROUTING_HANDLER_H
#define FUNCTION_WRAPPER_INTERCEPT_ROUTING_HANDLER_H

#include "TrampolineBridge/ClosureTrampolineBridge/ClosureTrampoline.h"
#include "Interceptor.h"
#include "dobby_internal.h"

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

// Dispatch the routing befor running the origin function
void prologue_routing_dispatch(DobbyRegisterContext *ctx, ClosureTrampolineEntry *entry);

// Dispatch the routing before the function return . (as it's implementation by relpace `return address` in the stack
// ,or LR register)
void epilogue_routing_dispatch(DobbyRegisterContext *ctx, ClosureTrampolineEntry *entry);

void pre_call_forward_handler(DobbyRegisterContext *ctx, InterceptEntry *entry);

void post_call_forward_handler(DobbyRegisterContext *ctx, InterceptEntry *entry);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif