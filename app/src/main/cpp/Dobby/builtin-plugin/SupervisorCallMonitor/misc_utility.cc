#include "misc_utility.h"

#include <string.h>

segment_command_t *macho_kit_get_segment_by_name(mach_header_t *header, const char *segname) {
  segment_command_t *curr_seg_cmd = NULL;

  curr_seg_cmd = (segment_command_t *)((addr_t)header + sizeof(mach_header_t));
  for (int i = 0; i < header->ncmds; i++) {
    if (curr_seg_cmd->cmd == LC_SEGMENT_ARCH_DEPENDENT) {
      if (!strncmp(curr_seg_cmd->segname, segname, sizeof(curr_seg_cmd->segname))) {
        break;
      }
    }
    curr_seg_cmd = (segment_command_t *)((addr_t)curr_seg_cmd + curr_seg_cmd->cmdsize);
  }

  return curr_seg_cmd;
}

section_t *macho_kit_get_section_by_name(mach_header_t *header, const char *segname, const char *sectname) {
  section_t *section = NULL;
  segment_command_t *segment = NULL;

  int i = 0;

  segment = macho_kit_get_segment_by_name(header, segname);
  if (!segment)
    goto finish;

  section = (section_t *)((addr_t)segment + sizeof(segment_command_t));
  for (i = 0; i < segment->nsects; ++i) {
    if (!strncmp(section->sectname, sectname, sizeof(section->sectname))) {
      break;
    }
    section += 1;
  }
  if (i == segment->nsects) {
    section = NULL;
  }

finish:
  return section;
}