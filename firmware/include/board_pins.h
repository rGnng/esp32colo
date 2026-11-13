#pragma once

// Working custom configuration for the tested unit. Only the subset in
// docs/gpio-map.md is established by factory-code analysis; remaining names
// do not establish physical wiring or component identity.
namespace Board {
constexpr int kKey1 = 6, kKey2 = 1, kKey3 = 4, kKey4 = 5;
constexpr int kKnobButton = 46, kOpenButton = 15, kRecoveryButton = 0;
constexpr int kEncoderA = 9, kEncoderB = 10;
constexpr int kBacklight = 18, kLcdTe = 17;
constexpr int kRgb = 48;
// Factory qt.init(48) + qt.new(14). Ring/key grouping is a custom assignment.
constexpr int kRingPixels = 9, kKeyPixels = 4;
constexpr int kKeyPixelStart = kRingPixels;
constexpr int kPixelCount = 14;
constexpr int kSupplyAdc = 7, kLightAdc = 16, kCharge = 3;
constexpr int kSda = 42, kScl = 41;
constexpr int kMclk = 40, kBclk = 39, kLrck = 47;
constexpr int kAudioOut = 21, kAudioIn = 38;
constexpr int kAmpEnable = 2;
// Held high by the custom driver before LED initialization. The factory
// writes this GPIO low/high; its physical circuit role remains unrecovered.
constexpr int kLedPowerEnable = 8;
}  // namespace Board
