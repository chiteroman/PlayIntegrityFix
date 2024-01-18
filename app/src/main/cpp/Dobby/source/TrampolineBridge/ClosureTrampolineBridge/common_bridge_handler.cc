#include "logging/logging.h"

#include "TrampolineBridge/ClosureTrampolineBridge/common_bridge_handler.h"

PUBLIC void common_closure_bridge_handler(DobbyRegisterContext *ctx, ClosureTrampolineEntry *entry) {
  DLOG(0, "common bridge handler: carry data: %p, carry handler: %p", (InterceptEntry *)entry->carry_data,
       entry->carry_handler);

  typedef void (*routing_handler_t)(InterceptEntry *, DobbyRegisterContext *);
  auto routing_handler = (routing_handler_t)entry->carry_handler;

#if defined(__APPLE__) && __arm64e__
#if __has_feature(ptrauth_calls)
  uint64_t discriminator = 0;
  // discriminator = __builtin_ptrauth_type_discriminator(__typeof(routing_handler));
  routing_handler = (__typeof(routing_handler))__builtin_ptrauth_sign_unauthenticated((void *)routing_handler,
                                                                                      ptrauth_key_asia, discriminator);
#endif
#endif

  routing_handler((InterceptEntry *)entry->carry_data, ctx);
}
