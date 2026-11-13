#pragma once
#include <cmath>
#include <stdint.h>

namespace DemoLevels {
// ES8311 DAC register 0x32: 0xBF = 0 dB, each step is 0.5 dB.
// Treat the UI percentage as linear signal amplitude, never positive gain.
inline uint8_t codecVolume(int percent) {
  if (percent <= 0) return 0;
  if (percent >= 100) return 0xBF;
  const long value = lroundf(191 + 40 * log10f(percent / 100.0f));
  return value < 1 ? 1 : value > 191 ? 191 : value;
}

// Apply this AFTER gamma correction of full-scale RGB. Applying gamma to
// an already dimmed color quantizes the 0..64 brightness range near zero.
inline uint32_t scaleRgb(uint32_t color, uint8_t brightness) {
  const uint32_t r = (((color >> 16) & 255) * brightness + 127) / 255;
  const uint32_t g = (((color >> 8) & 255) * brightness + 127) / 255;
  const uint32_t b = ((color & 255) * brightness + 127) / 255;
  return (r << 16) | (g << 8) | b;
}
}  // namespace DemoLevels
