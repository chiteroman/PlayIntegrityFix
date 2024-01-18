#ifndef CORE_CODEGEN_H
#define CORE_CODEGEN_H

#include "core/assembler/assembler.h"

using namespace zz;

class CodeGenBase {
public:
  CodeGenBase(AssemblerBase *assembler) : assembler_(assembler) {
  }

protected:
  AssemblerBase *assembler_;
};

#endif