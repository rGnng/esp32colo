#pragma once
#include <cstdint>

namespace LightingSettings {
enum Mode : uint32_t { kRainbow, kStatic };

// One NVS blob commits all lighting settings together. Brightness is percent.
struct Stored {
  uint32_t version = 1;
  uint32_t screen = 70, ring = 70, keys = 70;
  uint32_t autoScreen = 0, autoRing = 0, autoKeys = 0;
  uint32_t mode = kRainbow, color = 0xFFFFFF;
  uint32_t darkRaw = 0, brightRaw = 1200;
};
static_assert(sizeof(Stored) == 44, "lighting settings storage layout changed");

inline bool valid(const Stored &s) {
  return s.version == 1 && s.screen <= 100 && s.ring <= 100 && s.keys <= 100 &&
    s.autoScreen <= 1 && s.autoRing <= 1 && s.autoKeys <= 1 &&
    s.mode <= kStatic && s.color <= 0xFFFFFF && s.darkRaw <= 4095 &&
    s.brightRaw <= 4095 && s.darkRaw != s.brightRaw;
}

// Calibration permits either sensor polarity. Auto ranges from 10% of the
// selected brightness in darkness to the full selected brightness in light.
inline float factor(float raw, uint32_t dark, uint32_t bright) {
  float fraction = (raw - float(dark)) / (float(bright) - float(dark));
  if (fraction < 0) fraction = 0;
  if (fraction > 1) fraction = 1;
  return 0.1f + fraction * 0.9f;
}

inline int level(uint32_t selected, bool automatic, bool sensorUsable, float scale) {
  return int(selected * (automatic && sensorUsable ? scale : 1.0f) + 0.5f);
}

inline uint8_t pwm(int percent) { return (percent * 255 + 50) / 100; }
}  // namespace LightingSettings
