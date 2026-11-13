#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Recovered from factory qt:write(), 0x4201e33c..0x4201e452.
// See docs/firmware-led-evidence.md for the configuration / lookup table.
namespace FactoryLedFrame {
constexpr size_t kPixels = 14, kBytes = 480, kPrefixBytes = 72;
constexpr uint32_t kSpiHz = 3200000;

inline void channel(uint8_t value, uint8_t *output) {
  // At 3.2 MHz: 0 -> 1000 (312.5 ns high), 1 -> 1100 (625 ns high).
  for (int pair = 0; pair < 4; ++pair) {
    const uint8_t first = (value & 0x80) ? 0xC : 0x8;
    const uint8_t second = (value & 0x40) ? 0xC : 0x8;
    output[pair] = (first << 4) | second;
    value <<= 2;
  }
}

inline bool encode(const uint32_t *rgb, size_t count, uint8_t *output, size_t size) {
  if (count > kPixels || size < kBytes) return false;
  memset(output, 0, kBytes);
  for (size_t i = 0; i < count; ++i) {
    uint8_t *pixel = output + kPrefixBytes + i * 12;
    channel((rgb[i] >> 8) & 255, pixel);      // G
    channel((rgb[i] >> 16) & 255, pixel + 4); // R
    channel(rgb[i] & 255, pixel + 8);        // B
  }
  return true;
}
}  // namespace FactoryLedFrame
