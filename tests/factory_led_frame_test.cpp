#include "factory_led_frame.h"
#include <cassert>
#include <iostream>

int main() {
  // Independent fixture from factory initialized DRAM at 0x3fca0a58.
  // LE uint16 values; full dump offset 0x114464, SHA-256 9c73cc4e...46e.
  const uint16_t lookup[] = {0x8888, 0x8C88, 0xC888, 0xCC88,
    0x888C, 0x8C8C, 0xC88C, 0xCC8C, 0x88C8, 0x8CC8, 0xC8C8, 0xCCC8,
    0x88CC, 0x8CCC, 0xC8CC, 0xCCCC};
  uint8_t encoded[4];
  for (unsigned value = 0; value < 256; ++value) {
    FactoryLedFrame::channel(value, encoded);
    const uint16_t high = lookup[value >> 4], low = lookup[value & 15];
    assert(encoded[0] == (high & 255) && encoded[1] == (high >> 8));
    assert(encoded[2] == (low & 255) && encoded[3] == (low >> 8));
  }
  uint32_t pixels[14] = {};
  uint8_t frame[480];
  pixels[0] = 0x123456;
  pixels[13] = 0xFFFFFF;
  assert(FactoryLedFrame::encode(pixels, 14, frame, sizeof(frame)));
  for (unsigned i = 0; i < 72; ++i) assert(frame[i] == 0);
  for (unsigned i = 72 + 14 * 12; i < 480; ++i) assert(frame[i] == 0);
  // GRB channel order and the final slot are not dropped.
  FactoryLedFrame::channel(0x34, encoded);
  assert(!memcmp(frame + 72, encoded, 4));
  FactoryLedFrame::channel(0x12, encoded);
  assert(!memcmp(frame + 76, encoded, 4));
  FactoryLedFrame::channel(0x56, encoded);
  assert(!memcmp(frame + 80, encoded, 4));
  for (unsigned i = 72 + 13 * 12; i < 72 + 14 * 12; ++i) assert(frame[i] == 0xCC);
  assert(!FactoryLedFrame::encode(pixels, 15, frame, sizeof(frame)));
  assert(!FactoryLedFrame::encode(pixels, 14, frame, 479));
  std::cout << "PASS factory LED lookup (256 values), GRB order, 14 slots and reset padding\n";
}
