#pragma once

#include <stdint.h>
typedef uintptr_t addr_t;

#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#if defined(__LP64__)
typedef struct mach_header_64 mach_header_t;
typedef struct segment_command_64 segment_command_t;
typedef struct section_64 section_t;
typedef struct nlist_64 nlist_t;
#define LC_SEGMENT_ARCH_DEPENDENT LC_SEGMENT_64
#else
typedef struct mach_header mach_header_t;
typedef struct segment_command segment_command_t;
typedef struct section section_t;
typedef struct nlist nlist_t;
#define LC_SEGMENT_ARCH_DEPENDENT LC_SEGMENT
#endif

// get macho segment by segment name
segment_command_t *macho_kit_get_segment_by_name(mach_header_t *mach_header, const char *segname);

// get macho section by segment name and section name
section_t *macho_kit_get_section_by_name(mach_header_t *mach_header, const char *segname, const char *sectname);
