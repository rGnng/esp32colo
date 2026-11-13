#pragma once
#include <Arduino.h>

namespace AudioDemo {
struct Snapshot {
  bool i2c = false, codec = false, i2s = false, task = false;
  uint8_t address = 0, idHigh = 0, idLow = 0, version = 0;
  int error = 0, volume = 75;
  uint8_t dacVolumeRegister = 0;
  uint16_t lastToneHz=0,lastToneDurationMs=0;
  uint32_t rxFrames = 0, txFrames = 0, ioErrors = 0, tonesPlayed = 0;
  float rms = 0, peak = 0, dbfs = -96;
  int16_t waveform[64] = {};
  bool tone = false;
};
void begin();
Snapshot snapshot();
void beep(unsigned frequency = 660, unsigned durationMs = 250);
void setVolume(int percent);
void pause(bool paused);
String statusJson();
}  // namespace AudioDemo
