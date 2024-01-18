#include "dobby_internal.h"

#include "InterceptRouting/Routing/InstructionInstrument/InstructionInstrumentRouting.h"
#include "InterceptRouting/Routing/InstructionInstrument/instrument_routing_handler.h"

#include "TrampolineBridge/ClosureTrampolineBridge/common_bridge_handler.h"

void instrument_forward_handler(InterceptEntry *entry, DobbyRegisterContext *ctx) {
  auto routing = static_cast<InstructionInstrumentRouting *>(entry->routing);
  if (routing->pre_handler) {
    auto handler = (dobby_instrument_callback_t)routing->pre_handler;
    (*handler)((void *)entry->patched_addr, ctx);
  }

  // set prologue bridge next hop address as relocated instructions
  set_routing_bridge_next_hop(ctx, (void *)entry->relocated_addr);
}

void instrument_routing_dispatch(InterceptEntry *entry, DobbyRegisterContext *ctx) {
  instrument_forward_handler(entry, ctx);
}
