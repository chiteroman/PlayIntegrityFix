#pragma once

#include "macho_ctx.h"

uintptr_t macho_file_memory_symbol_resolve(cpu_type_t cpu, cpu_subtype_t subtype, const uint8_t *file_mem,
                                           char *symbol_name_pattern);

uintptr_t macho_file_symbol_resolve(cpu_type_t cpu, cpu_subtype_t subtype, const char *file, char *symbol_name_pattern);
