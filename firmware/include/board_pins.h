#pragma once

// ColoPLAY V1. See docs/gpio-map.md and firmware-gpio-evidence.md.
// GPIO26..37 belong to flash / Octal PSRAM on the N16R8 module.
namespace Board {
constexpr int kKey1 = 6, kKey2 = 1, kKey3 = 4, kKey4 = 5;
constexpr int kKnobButton = 46, kOpenButton = 15, kRecoveryButton = 0;
constexpr int kEncoderA = 9, kEncoderB = 10;
constexpr int kBacklight = 18, kLcdTe = 17;
constexpr int kRgb = 48, kPixelCount = 9; // parallel chains of 4 and 9
constexpr int kSupplyAdc = 7, kLightAdc = 16, kCharge = 3;
constexpr int kSda = 42, kScl = 41;
constexpr int kMclk = 40, kBclk = 39, kLrck = 47;
constexpr int kAudioOut = 21, kAudioIn = 38;
constexpr int kAmpEnable = 2, kMicBias = 8;
}  // namespace Board
