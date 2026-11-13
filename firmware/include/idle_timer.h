#pragma once
#include <cstdint>

// Unsigned elapsed time keeps inactivity timing valid across millis() rollover.
class IdleTimer {
 public:
  void configure(uint32_t minutes,uint32_t now){interval_=minutes*60000u;activity(now);}
  void activity(uint32_t now){lastActivity_=now;}
  bool due(uint32_t now) const {return interval_&&elapsed(now)>=interval_;}
  uint32_t elapsed(uint32_t now) const {return now-lastActivity_;}
 private:
  uint32_t interval_=3600000u,lastActivity_=0;
};
