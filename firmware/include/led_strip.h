#pragma once
#include <Arduino.h>

namespace LedStrip {
bool begin();
bool ready();
void setPixel(size_t index, uint32_t rgb);
void show();
int error();
uint32_t transfers();
uint32_t errors();
}  // namespace LedStrip
