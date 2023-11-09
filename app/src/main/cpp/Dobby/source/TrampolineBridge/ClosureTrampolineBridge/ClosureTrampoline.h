#pragma once

#include "dobby/dobby_internal.h"

#ifdef ENABLE_CLOSURE_TRAMPOLINE_TEMPLATE
#ifdef __cplusplus
extern "C" {
#endif //__cplusplus
void closure_trampoline_template();
void closure_bridge_template();
#ifdef __cplusplus
}
#endif //__cplusplus
#endif

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

typedef struct {
  void *address;
  int size;
  void *carry_handler;
  void *carry_data;
} ClosureTrampolineEntry;

asm_func_t get_closure_bridge();

#ifdef __cplusplus
}
#endif //__cplusplus

class ClosureTrampoline {
private:
  static tinystl::vector<ClosureTrampolineEntry> *trampolines_;

public:
  static ClosureTrampolineEntry *CreateClosureTrampoline(void *carry_data, void *carry_handler);
};
