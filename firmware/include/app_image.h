#pragma once
#include <stddef.h>
#include <stdint.h>

namespace AppImage {
// ESP image header (24), first segment header (8), app descriptor magic (4).
constexpr size_t kPrefixBytes = 36;
inline bool validPrefix(const uint8_t *prefix, size_t size) {
  if (size < kPrefixBytes) return false;
  const uint32_t descriptor = uint32_t(prefix[32]) | (uint32_t(prefix[33]) << 8) |
                              (uint32_t(prefix[34]) << 16) | (uint32_t(prefix[35]) << 24);
  const uint32_t segmentSize = uint32_t(prefix[28]) | (uint32_t(prefix[29]) << 8) |
                               (uint32_t(prefix[30]) << 16) | (uint32_t(prefix[31]) << 24);
  return prefix[0] == 0xE9 && prefix[1] > 0 && prefix[1] <= 16 &&
         prefix[12] == 9 && prefix[13] == 0 && segmentSize >= 256 &&
         descriptor == 0xABCD5432;
}
}  // namespace AppImage
