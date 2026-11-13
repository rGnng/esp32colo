#include "lighting_settings.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
  using namespace LightingSettings;
  Stored settings;
  assert(valid(settings));
  assert(settings.screen == 70 && settings.ring == 70 && settings.keys == 70);
  assert(!settings.autoScreen && !settings.autoRing && !settings.autoKeys);
  assert(pwm(0) == 0 && pwm(70) == 179 && pwm(100) == 255);

  // Either calibration polarity must give the same dim-to-bright response,
  // including readings outside the captured endpoints.
  for (bool reverse : {false, true}) {
    const uint32_t dark = reverse ? 3000 : 500, bright = reverse ? 500 : 3000;
    assert(std::abs(factor(dark, dark, bright) - 0.1f) < 0.0001f);
    assert(std::abs(factor(bright, dark, bright) - 1.0f) < 0.0001f);
    assert(std::abs(factor(1750, dark, bright) - 0.55f) < 0.0001f);
    assert(factor(reverse ? 4095 : 0, dark, bright) == 0.1f);
    assert(factor(reverse ? 0 : 4095, dark, bright) == 1.0f);
  }
  const float scale = factor(0, 0, 1200);
  assert(level(70, true, true, scale) == 7);
  assert(level(40, true, true, scale) == 4);
  assert(level(70, false, true, scale) == 70);
  assert(level(70, true, false, scale) == 70);  // missing/stale sensor fallback
  assert(level(0, true, true, 1) == 0);
  for (int raw = 0; raw <= 4095; ++raw) {
    const float f = factor(raw, 0, 1200);
    assert(level(70, true, true, f) >= 7 && level(70, true, true, f) <= 70);
  }

  settings.darkRaw = settings.brightRaw;
  assert(!valid(settings));
  settings = Stored{}; settings.ring = 101; assert(!valid(settings));
  settings = Stored{}; settings.autoKeys = 2; assert(!valid(settings));
  settings = Stored{}; settings.mode = 2; assert(!valid(settings));
  settings = Stored{}; settings.color = 0x1000000; assert(!valid(settings));
  settings = Stored{}; settings.brightRaw = 4096; assert(!valid(settings));
  std::cout << "PASS lighting defaults, calibration polarity/clamps, independent levels and sensor fallback\n";
}
