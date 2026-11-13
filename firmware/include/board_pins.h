#pragma once

// ColoPLAY V1. See docs/gpio-map.md and firmware-gpio-evidence.md.
// GPIO26..37 belong to flash / Octal PSRAM on the N16R8 module.
namespace Board {
constexpr int kKey1 = 6, kKey2 = 1, kKey3 = 4, kKey4 = 5;
constexpr int kKnobButton = 46, kOpenButton = 15, kRecoveryButton = 0;
constexpr int kEncoderA = 9, kEncoderB = 10;
constexpr int kBacklight = 18, kLcdTe = 17;
constexpr int kRgb = 48;
// Factory qt.init(48) + qt.new(14). Physical index mapping is tested separately.
constexpr int kRingPixels = 9, kKeyPixels = 4;
constexpr int kKeyPixelStart = kRingPixels;
constexpr int kPixelCount = 14;
constexpr int kSupplyAdc = 7, kLightAdc = 16, kCharge = 3;
constexpr int kSda = 42, kScl = 41;
constexpr int kMclk = 40, kBclk = 39, kLrck = 47;
constexpr int kAudioOut = 21, kAudioIn = 38;
constexpr int kAmpEnable = 2;
// Schematic net MICBIAS: Q7/Q6 enable VCCL, and the net biases D14.
// The analog microphone bias comes from the ES8311 ADCV network.
constexpr int kLedPowerEnable = 8;
}  // namespace Board
