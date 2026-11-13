#include "clock_settings.h"
#include <cassert>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>

int main() {
  using namespace ClockSettings;
  assert(valid(Stored{}));
  for (const char *server : {"pool.ntp.org", "time.cloudflare.com", "10.0.0.1", "ntp", "ntp.lan."})
    assert(validServer(server));
  for (const char *server : {"", "https://pool.ntp.org", "ntp:123", "bad host", "bad\nntp",
                             "a..b", ".ntp", "-ntp.lan", "ntp-.lan", "bad\"host"})
    assert(!validServer(server));
  assert(validServer(std::string(63, 'a').c_str()));
  assert(!validServer(std::string(64, 'a').c_str()));
  assert(!validServer(std::string(254, 'a').c_str()));
  Stored damaged;
  damaged.version = 99;
  assert(!valid(damaged));
  damaged = Stored{};
  std::memset(damaged.ntpServer, 'a', sizeof(damaged.ntpServer));
  assert(!valid(damaged));
  damaged = Stored{};
  std::strcpy(damaged.timezone, "unknown");
  assert(!valid(damaged));
  LegacyStored legacy;
  std::strcpy(legacy.ntpServer,"10.0.0.5"); std::strcpy(legacy.timezone,"Asia/Shanghai");
  Stored migrated;
  assert(migrate(legacy,migrated) && valid(migrated));
  assert(!std::strcmp(migrated.ntpServer,"10.0.0.5") && !std::strcmp(migrated.timezone,"Asia/Shanghai"));
  assert(!migrated.ntpServer2[0] && !migrated.ntpServer3[0]);
  std::strcpy(migrated.ntpServer2,"ntp.lan"); std::strcpy(migrated.ntpServer3,"10.0.0.6"); assert(valid(migrated));
  std::strcpy(migrated.ntpServer2,"http://bad"); assert(!valid(migrated));
  std::memset(legacy.ntpServer,'a',sizeof(legacy.ntpServer)); assert(!migrate(legacy,migrated));
  // Verify the embedded POSIX rules, including the half-hour offset and DST.
  for (const auto &zone : kZones) {
    setenv("TZ", zone.posix, 1); tzset();
    for (const time_t instant : {time_t(1767225600), time_t(1782864000)}) {
      tm local;
      localtime_r(&instant, &local);
      assert(mktime(&local) == instant);
      if (!std::strncmp(zone.id,"UTC",3)) {
        char offset[8];strftime(offset,sizeof(offset),"%z",&local);
        std::string expected=std::string(zone.id+3,3)+std::string(zone.id+7,2);
        assert(expected==offset && local.tm_isdst==0);
      }
    }
  }
  const time_t winter = 1767225600;  // 2026-01-01 00:00 UTC
  tm local;
  setenv("TZ", findZone("Asia/Hong_Kong")->posix, 1); tzset();
  localtime_r(&winter, &local); assert(local.tm_hour == 8 && local.tm_mday == 1);
  setenv("TZ", findZone("Asia/Kolkata")->posix, 1); tzset();
  localtime_r(&winter, &local); assert(local.tm_hour == 5 && local.tm_min == 30);
  setenv("TZ", findZone("America/Los_Angeles")->posix, 1); tzset();
  localtime_r(&winter, &local); assert(local.tm_hour == 16 && local.tm_mday == 31 && local.tm_isdst == 0);
  const time_t summer = 1782864000;  // 2026-07-01 00:00 UTC
  localtime_r(&summer, &local); assert(local.tm_hour == 17 && local.tm_isdst == 1);
  std::cout << "PASS clock hostname/storage validation, offsets and daylight saving\n";
}
