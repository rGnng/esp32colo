#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ClockSettings {
constexpr char kDefaultServer[] = "pool.ntp.org";
struct Zone { const char *id; const char *label; const char *posix; };
constexpr Zone kZones[] = {
  {"Etc/UTC", "UTC+00:00", "UTC0"},
  {"Asia/Hong_Kong", "Hong Kong (UTC+08:00)", "HKT-8"},
  {"Asia/Shanghai", "Shanghai (UTC+08:00)", "CST-8"},
  {"Asia/Taipei", "Taipei (UTC+08:00)", "CST-8"},
  {"Asia/Tokyo", "Tokyo (UTC+09:00)", "JST-9"},
  {"Asia/Singapore", "Singapore (UTC+08:00)", "SGT-8"},
  {"Asia/Kolkata", "Kolkata (UTC+05:30)", "IST-5:30"},
  {"America/Los_Angeles", "Los Angeles (UTC-08:00 / -07:00)", "PST8PDT,M3.2.0,M11.1.0"},
  {"America/New_York", "New York (UTC-05:00 / -04:00)", "EST5EDT,M3.2.0,M11.1.0"},
  {"Europe/London", "London (UTC+00:00 / +01:00)", "GMT0BST,M3.5.0/1,M10.5.0"},
  {"Europe/Berlin", "Berlin (UTC+01:00 / +02:00)", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Australia/Sydney", "Sydney (UTC+10:00 / +11:00)", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
  {"Pacific/Auckland", "Auckland (UTC+12:00 / +13:00)", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
  // Explicit fixed offsets use the reverse POSIX sign and never apply DST.
  {"UTC-12:00", "UTC-12:00", "<-12>12"},
  {"UTC-11:00", "UTC-11:00", "<-11>11"},
  {"UTC-10:00", "UTC-10:00", "<-10>10"},
  {"UTC-09:00", "UTC-09:00", "<-09>9"},
  {"UTC-08:00", "UTC-08:00", "<-08>8"},
  {"UTC-07:00", "UTC-07:00", "<-07>7"},
  {"UTC-06:00", "UTC-06:00", "<-06>6"},
  {"UTC-05:00", "UTC-05:00", "<-05>5"},
  {"UTC-04:00", "UTC-04:00", "<-04>4"},
  {"UTC-03:00", "UTC-03:00", "<-03>3"},
  {"UTC-02:00", "UTC-02:00", "<-02>2"},
  {"UTC-01:00", "UTC-01:00", "<-01>1"},
  {"UTC+01:00", "UTC+01:00", "<+01>-1"},
  {"UTC+02:00", "UTC+02:00", "<+02>-2"},
  {"UTC+03:00", "UTC+03:00", "<+03>-3"},
  {"UTC+04:00", "UTC+04:00", "<+04>-4"},
  {"UTC+05:00", "UTC+05:00", "<+05>-5"},
  {"UTC+06:00", "UTC+06:00", "<+06>-6"},
  {"UTC+07:00", "UTC+07:00", "<+07>-7"},
  {"UTC+08:00", "UTC+08:00", "<+08>-8"},
  {"UTC+09:00", "UTC+09:00", "<+09>-9"},
  {"UTC+10:00", "UTC+10:00", "<+10>-10"},
  {"UTC+11:00", "UTC+11:00", "<+11>-11"},
  {"UTC+12:00", "UTC+12:00", "<+12>-12"},
  {"UTC+13:00", "UTC+13:00", "<+13>-13"},
  {"UTC+14:00", "UTC+14:00", "<+14>-14"},
  {"UTC+05:30", "UTC+05:30", "<+0530>-5:30"},
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

// Version 2 adds two optional failover servers; version 1 is migrated on load.
struct LegacyStored {
  uint16_t version = 1;
  char ntpServer[254] = "pool.ntp.org";
  char timezone[40] = "Etc/UTC";
};
struct Stored {
  uint16_t version = 2;
  char ntpServer[254] = "pool.ntp.org";
  char ntpServer2[254] = "";
  char ntpServer3[254] = "";
  char timezone[40] = "Etc/UTC";
};
static_assert(sizeof(Stored)==804 && sizeof(LegacyStored)==296,"clock settings storage layout changed");
inline bool valid(const Stored &s) {
  return s.version==2 && std::memchr(s.ntpServer,0,sizeof(s.ntpServer)) &&
    std::memchr(s.ntpServer2,0,sizeof(s.ntpServer2)) && std::memchr(s.ntpServer3,0,sizeof(s.ntpServer3)) &&
    std::memchr(s.timezone,0,sizeof(s.timezone)) && validServer(s.ntpServer) &&
    (!s.ntpServer2[0]||validServer(s.ntpServer2)) && (!s.ntpServer3[0]||validServer(s.ntpServer3)) && findZone(s.timezone);
}
inline bool migrate(const LegacyStored &old, Stored &next) {
  if (old.version!=1||!std::memchr(old.ntpServer,0,sizeof(old.ntpServer))||
      !std::memchr(old.timezone,0,sizeof(old.timezone))||!validServer(old.ntpServer)||!findZone(old.timezone)) return false;
  next=Stored{}; std::strcpy(next.ntpServer,old.ntpServer); std::strcpy(next.timezone,old.timezone); return true;
}
}  // namespace ClockSettings
