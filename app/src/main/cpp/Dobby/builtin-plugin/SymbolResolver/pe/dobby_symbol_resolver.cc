#include "SymbolResolver/dobby_symbol_resolver.h"
#include "dobby/common.h"

#include <windows.h>

#include <string>
#include <string.h>

#include "PlatformUtil/ProcessRuntimeUtility.h"

#include <vector>

#undef LOG_TAG
#define LOG_TAG "DobbySymbolResolver"

PUBLIC void *DobbySymbolResolver(const char *image_name, const char *symbol_name_pattern) {
  void *result = NULL;

  HMODULE hMod = LoadLibraryExA(image_name, NULL, DONT_RESOLVE_DLL_REFERENCES);
  result = GetProcAddress(hMod, symbol_name_pattern);
  if (result)
    return result;

  //result = resolve_elf_internal_symbol(image_name, symbol_name_pattern);
  return result;
}