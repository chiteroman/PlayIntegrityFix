#ifndef FUNCTION_WRAPPER_H
#define FUNCTION_WRAPPER_H

#include "dobby/dobby_internal.h"

#include "TrampolineBridge/ClosureTrampolineBridge/ClosureTrampoline.h"
#include "InterceptRouting/InterceptRouting.h"
#include "Interceptor.h"

#if TARGET_ARCH_IA32
#elif TARGET_ARCH_X64
#include "InterceptRouting/x64/X64InterceptRouting.h"
#elif TARGET_ARCH_ARM64
#include "InterceptRouting/arm64/ARM64InterceptRouting.h"
#elif TARGET_ARCH_ARM
#else
#error "unsupported architecture"
#endif

class FunctionWrapperRouting : public InterceptRouting {
public:
  FunctionWrapperRouting(InterceptEntry *entry) : InterceptRouting(entry) {
  }

  void DispatchRouting();

  void *GetTrampolineTarget();

private:
  void BuildPreCallRouting();

  void BuildPostCallRouting();

private:
  void *prologue_dispatch_bridge;

  void *epilogue_dispatch_bridge;
};

#endif
