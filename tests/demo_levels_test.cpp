#include "demo_levels.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main() {
  assert(DemoLevels::codecVolume(0) == 0);
  assert(DemoLevels::codecVolume(100) == 191);
  assert(DemoLevels::codecVolume(200) == 191);
  int previous = 0;
  for (int percent = 1; percent <= 100; ++percent) {
    const int value = DemoLevels::codecVolume(percent);
    const float gain = (value - 191) * 0.5f;
    assert(value >= previous && value <= 191);
    assert(std::abs(gain - 20 * std::log10(percent / 100.0)) <= 0.251);
    previous = value;
  }
  assert(DemoLevels::scaleRgb(0xFFFFFF, 40) == 0x282828);
  assert(DemoLevels::scaleRgb(0xFF0000, 40) == 0x280000);
  assert(DemoLevels::scaleRgb(0x00FF00, 64) == 0x004000);
  assert(DemoLevels::scaleRgb(0xFFFFFF, 0) == 0);
  assert(DemoLevels::scaleRgb(0x123456, 255) == 0x123456);
  std::cout << "PASS codec gain bounds/steps and RGB brightness scaling\n";
}
