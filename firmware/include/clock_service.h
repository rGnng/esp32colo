#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <ctime>

namespace ClockService {
struct Snapshot {
  bool valid = false, synced = false, connected = false;
  uint32_t unixTime = 0, lastSync = 0, syncCount = 0;
  tm local = {};
  char hhmm[6] = "--:--", seconds[3] = "--";
  char weekday[12] = "CLOCK", date[24] = "Waiting for time";
  char zone[12] = "UTC", iso[32] = "";
};
void begin();
void tick();
void requestSync();
Snapshot snapshot();
String statusJson(bool includeZones = false);
bool configure(WebServer &server, String &error);
}  // namespace ClockService
