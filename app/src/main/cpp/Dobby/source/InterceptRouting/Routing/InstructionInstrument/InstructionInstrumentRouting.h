#pragma once

#include "dobby/dobby_internal.h"

#include "InterceptRouting/InterceptRouting.h"

#include "TrampolineBridge/ClosureTrampolineBridge/ClosureTrampoline.h"

class InstructionInstrumentRouting : public InterceptRouting {
public:
  InstructionInstrumentRouting(InterceptEntry *entry, dobby_instrument_callback_t pre_handler,
                               dobby_instrument_callback_t post_handler)
      : InterceptRouting(entry) {
    this->prologue_dispatch_bridge = nullptr;
    this->pre_handler = pre_handler;
    this->post_handler = post_handler;
  }

  void DispatchRouting() override;

private:
  void BuildRouting();

public:
  dobby_instrument_callback_t pre_handler;
  dobby_instrument_callback_t post_handler;

private:
  void *prologue_dispatch_bridge;
};
