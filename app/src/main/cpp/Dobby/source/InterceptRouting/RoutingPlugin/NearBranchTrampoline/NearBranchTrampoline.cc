#include "InterceptRouting/RoutingPlugin/NearBranchTrampoline/NearBranchTrampoline.h"

#include "dobby_internal.h"

#include "MemoryAllocator/NearMemoryAllocator.h"

#include "InterceptRouting/RoutingPlugin/RoutingPlugin.h"

using namespace zz;

PUBLIC void dobby_enable_near_branch_trampoline() {
  RoutingPluginInterface *plugin = new NearBranchTrampolinePlugin;
  RoutingPluginManager::registerPlugin("near_branch_trampoline", plugin);
  RoutingPluginManager::near_branch_trampoline = plugin;
}

PUBLIC void dobby_disable_near_branch_trampoline() {
  NearBranchTrampolinePlugin *plugin = (NearBranchTrampolinePlugin *)RoutingPluginManager::near_branch_trampoline;
  delete plugin;
  RoutingPluginManager::near_branch_trampoline = NULL;
}

#if 0
int NearBranchTrampolinePlugin::PredefinedTrampolineSize() {
#if __arm64__
  return 4;
#elif __arm__
  return 4;
#endif
}
#endif

extern CodeBufferBase *GenerateNearTrampolineBuffer(InterceptRouting *routing, addr_t from, addr_t to);
bool NearBranchTrampolinePlugin::GenerateTrampolineBuffer(InterceptRouting *routing, addr_t src, addr_t dst) {
  CodeBufferBase *trampoline_buffer;
  trampoline_buffer = GenerateNearTrampolineBuffer(routing, src, dst);
  if (trampoline_buffer == NULL)
    return false;
  routing->SetTrampolineBuffer(trampoline_buffer);
  return true;
}

// generate trampoline, patch the original entry
bool NearBranchTrampolinePlugin::Active(InterceptRouting *routing) {
  InterceptEntry *entry = routing->GetInterceptEntry();
  return true;
}
