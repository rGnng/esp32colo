#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#define PROGMEM
class String {
  std::string value;
 public:
  String(const char *s = "") : value(s) {}
  bool isEmpty() const { return value.empty(); }
  size_t length() const { return value.size(); }
  const char *c_str() const { return value.c_str(); }
};
struct FakeSerial { template<class... T> void printf(const char *, T...) {} void println(const char *) {} };
struct FakeEsp { int restarts = 0; void restart() { ++restarts; } };
extern FakeSerial Serial;
extern FakeEsp ESP;
extern uint32_t testMillis;
inline uint32_t millis() { return testMillis; }
