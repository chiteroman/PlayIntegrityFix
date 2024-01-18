#pragma once

#include "dobby_internal.h"

extern "C" {
void instrument_routing_dispatch(InterceptEntry *entry, DobbyRegisterContext *ctx);
}