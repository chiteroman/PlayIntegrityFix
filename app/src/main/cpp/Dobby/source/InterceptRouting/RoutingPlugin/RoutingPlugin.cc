#include "InterceptRouting/RoutingPlugin/RoutingPlugin.h"

std::vector<RoutingPluginInterface *> RoutingPluginManager::plugins;

RoutingPluginInterface *RoutingPluginManager::near_branch_trampoline = NULL;

void RoutingPluginManager::registerPlugin(const char *name, RoutingPluginInterface *plugin) {
  DLOG(0, "register %s plugin", name);

  RoutingPluginManager::plugins.push_back(plugin);
}
