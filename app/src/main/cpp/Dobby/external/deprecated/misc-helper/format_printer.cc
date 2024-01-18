#include "misc-helper/format_printer.h"

void hexdump(const uint8_t *bytes, size_t len) {
  size_t ix;
  for (ix = 0; ix < len; ++ix) {
    if (ix != 0 && !(ix % 16))
      RAW_LOG(0, "\n");
    RAW_LOG(0, "%02X ", bytes[ix]);
  }
  RAW_LOG(0, "\n");
}
