#include "misc-helper/format_printer.h"

void hexdump(const uint8_t *bytes, size_t len) {
  size_t ix;
  for (ix = 0; ix < len; ++ix) {
    if (ix != 0 && !(ix % 16))
      LOG_FUNCTION_IMPL(0, "\n");
    LOG_FUNCTION_IMPL(0, "%02X ", bytes[ix]);
  }
  LOG_FUNCTION_IMPL(0, "\n");
}
