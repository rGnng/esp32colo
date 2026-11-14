#include "lighting_settings.h"
#include <cassert>
#include <cstring>
#include <iostream>

int main() {
  using namespace LightingSettings;
  Stored settings;
  assert(valid(settings));
  assert(settings.screen == 70 && settings.ring == 70 && settings.keys == 70);
  assert(sizeof(settings)==44);
  assert(pwm(0) == 0 && pwm(70) == 179 && pwm(100) == 255);

  // Actual v1 word offsets, including previously enabled Auto and captured
  // endpoints. Manual selections/color survive without interpreting them.
  const uint32_t legacy[]={1,21,46,83,1,1,1,kStatic,0x123456,500,3000};
  std::memcpy(&settings,legacy,sizeof(settings));
  assert(valid(settings)&&settings.screen==21&&settings.ring==46&&settings.keys==83);
  assert(settings.mode==kStatic&&settings.color==0x123456);
  assert(!std::memcmp(&settings,legacy,sizeof(settings)));
  for(auto &word:settings.reservedBeforeMode)word=0xFFFFFFFF;
  for(auto &word:settings.reservedTail)word=0xFFFFFFFF;
  assert(valid(settings)&&settings.screen==21&&settings.ring==46&&settings.keys==83);
  for(int p=0;p<=100;++p){settings.screen=p;settings.ring=100-p;settings.keys=42;assert(valid(settings));}
  settings=Stored{};settings.screen=101;assert(!valid(settings));
  settings=Stored{};settings.ring=101;assert(!valid(settings));
  settings=Stored{};settings.keys=101;assert(!valid(settings));
  settings=Stored{};settings.version=2;assert(!valid(settings));
  settings=Stored{};settings.mode=2;assert(!valid(settings));
  settings=Stored{};settings.color=0x1000000;assert(!valid(settings));
  std::cout << "PASS manual brightness defaults/bounds and byte-compatible legacy settings with retired fields ignored\n";
}
