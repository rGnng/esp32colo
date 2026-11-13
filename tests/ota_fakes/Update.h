#pragma once
#include "Arduino.h"
#include <vector>
constexpr int UPDATE_SIZE_UNKNOWN = -1, U_FLASH = 0;
struct FakeUpdate {
  bool running = false, endOk = true;
  int begins = 0, ends = 0, aborts = 0, commits = 0;
  std::vector<uint8_t> data;
  bool isRunning() const { return running; }
  void abort() { ++aborts; running = false; }
  bool begin(int, int) { ++begins; data.clear(); running = true; return true; }
  bool setMD5(const char *s) { return std::strlen(s) == 32; }
  size_t write(const uint8_t *p, size_t n) { data.insert(data.end(), p, p + n); return n; }
  bool end(bool) { ++ends; running = false; if (endOk) ++commits; return endOk; }
  const char *errorString() { return "test update error"; }
};
extern FakeUpdate Update;
