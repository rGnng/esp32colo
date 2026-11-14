#pragma once
#include <cstdint>

namespace LightingSettings {
enum Mode : uint32_t { kRainbow, kStatic };

// One NVS blob commits all lighting settings together. Brightness is percent.
struct Stored {
  uint32_t version = 1;
  uint32_t screen = 70, ring = 70, keys = 70;
  // Reserved words retain the old on-flash offsets. They are never applied.
  uint32_t reservedBeforeMode[3] = {};
  uint32_t mode = kRainbow, color = 0xFFFFFF;
  uint32_t reservedTail[2] = {0,1200};
};
static_assert(sizeof(Stored) == 44, "lighting settings storage layout changed");

inline bool valid(const Stored &s) {
  return s.version == 1 && s.screen <= 100 && s.ring <= 100 && s.keys <= 100 &&
    s.mode <= kStatic && s.color <= 0xFFFFFF;
}

inline uint8_t pwm(int percent) { return (percent * 255 + 50) / 100; }
}  // namespace LightingSettings
