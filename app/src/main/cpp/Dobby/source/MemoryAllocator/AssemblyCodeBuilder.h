#pragma once

#include "PlatformUnifiedInterface/MemoryAllocator.h"

#include "core/assembler/assembler.h"

using namespace zz;

using AssemblyCode = CodeMemBlock;

class AssemblyCodeBuilder {
public:
  static AssemblyCode *FinalizeFromTurboAssembler(AssemblerBase *assembler);
};
