#pragma once

#include "dobby/dobby_internal.h"

#include "InterceptRouting/RoutingPlugin/RoutingPlugin.h"

class NearBranchTrampolinePlugin : public RoutingPluginInterface {
  bool Prepare(InterceptRouting *routing) {
    return false;
  };

  bool Active(InterceptRouting *routing);

  bool GenerateTrampolineBuffer(InterceptRouting *routing, addr_t src, addr_t dst);
};
