#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ClockSettings {
constexpr char kDefaultServer[] = "pool.ntp.org";
struct Zone { const char *id; const char *label; const char *posix; };
constexpr Zone kZones[] = {
  {"Etc/UTC", "UTC", "UTC0"},
  {"Asia/Hong_Kong", "Hong Kong", "HKT-8"},
  {"Asia/Shanghai", "Shanghai", "CST-8"},
  {"Asia/Taipei", "Taipei", "CST-8"},
  {"Asia/Tokyo", "Tokyo", "JST-9"},
  {"Asia/Singapore", "Singapore", "SGT-8"},
  {"Asia/Kolkata", "Kolkata", "IST-5:30"},
  {"America/Los_Angeles", "Los Angeles", "PST8PDT,M3.2.0,M11.1.0"},
  {"America/New_York", "New York", "EST5EDT,M3.2.0,M11.1.0"},
  {"Europe/London", "London", "GMT0BST,M3.5.0/1,M10.5.0"},
  {"Europe/Berlin", "Berlin", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Australia/Sydney", "Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
  {"Pacific/Auckland", "Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
};

inline const Zone *findZone(const char *id) {
  for (const auto &zone : kZones) if (std::strcmp(id, zone.id) == 0) return &zone;
  return nullptr;
}

// SNTP accepts a hostname or IPv4 literal, without a scheme, port or path.
inline bool validServer(const char *server) {
  const size_t length = std::strlen(server);
  if (!length || length > 253) return false;
  size_t label = 0;
  for (size_t i = 0; i < length; ++i) {
    const char c = server[i];
    if (c == '.') {
      if (!label || server[i - 1] == '-') return false;
      label = 0;
    } else {
      const bool alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                         (c >= '0' && c <= '9');
      if ((!alnum && c != '-') || (!label && c == '-') || ++label > 63) return false;
    }
  }
  return server[length - 1] != '-';
}

// One NVS blob commits the server and timezone together.
struct Stored {
  uint16_t version = 1;
  char ntpServer[254] = "pool.ntp.org";
  char timezone[40] = "Etc/UTC";
};
static_assert(sizeof(Stored) == 296, "clock settings storage layout changed");

inline bool valid(const Stored &settings) {
  return settings.version == 1 &&
    std::memchr(settings.ntpServer, 0, sizeof(settings.ntpServer)) &&
    std::memchr(settings.timezone, 0, sizeof(settings.timezone)) &&
    validServer(settings.ntpServer) && findZone(settings.timezone);
}
}  // namespace ClockSettings
