#include "InterceptRouting/RoutingPlugin/RoutingPlugin.h"

tinystl::vector<RoutingPluginInterface *> RoutingPluginManager::plugins;

RoutingPluginInterface *RoutingPluginManager::near_branch_trampoline = NULL;

void RoutingPluginManager::registerPlugin(const char *name, RoutingPluginInterface *plugin) {
  DEBUG_LOG("register %s plugin", name);

  RoutingPluginManager::plugins.push_back(plugin);
}
