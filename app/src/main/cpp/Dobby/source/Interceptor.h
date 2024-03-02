#pragma once

#include "dobby/dobby_internal.h"
#include "InterceptEntry.h"

class Interceptor {
public:
  static Interceptor *SharedInstance();

public:
  InterceptEntry *find(addr_t addr);

  void remove(addr_t addr);

  void add(InterceptEntry *entry);

  const InterceptEntry *getEntry(int i);

  int count();

private:
  static Interceptor *instance;

  tinystl::vector<InterceptEntry *> entries;
};