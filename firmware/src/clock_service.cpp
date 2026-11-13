#include "device_log.h"
#include "clock_service.h"
#include "clock_settings.h"
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <atomic>
#include <cstdlib>

namespace ClockService {
namespace {
Preferences gPrefs;
ClockSettings::Stored gSettings;
bool gPrefsOpen = false, gStored = false, gConnected = false, gStarted = false;
std::atomic<uint32_t> gLastSync{0}, gSyncCount{0};
constexpr uint32_t kSyncIntervalMs = 60 * 60 * 1000;
constexpr time_t kEarliestTime = 1704067200;  // 2024-01-01; never show a 1970 clock.

void onSync(timeval *tv) {
  gLastSync.store(static_cast<uint32_t>(tv->tv_sec));
  gSyncCount.fetch_add(1);
  DeviceLog::println("[clock] NTP synchronized");
}

void applyZone() {
  const auto *zone = ClockSettings::findZone(gSettings.timezone);
  setenv("TZ", zone->posix, 1);
  tzset();
}
}  // namespace

void begin() {
  gPrefsOpen = gPrefs.begin("clock", false);
  ClockSettings::Stored stored;
  if (gPrefsOpen && gPrefs.getBytesLength("settings") == sizeof(stored) &&
      gPrefs.getBytes("settings", &stored, sizeof(stored)) == sizeof(stored) &&
      ClockSettings::valid(stored)) {
    gSettings = stored;
    gStored = true;
  } else if (gPrefsOpen) {
    ClockSettings::LegacyStored legacy;
    if (gPrefs.getBytesLength("settings")==sizeof(legacy) &&
        gPrefs.getBytes("settings",&legacy,sizeof(legacy))==sizeof(legacy)) ClockSettings::migrate(legacy,gSettings);
    gStored = gPrefs.putBytes("settings", &gSettings, sizeof(gSettings)) == sizeof(gSettings);
  }
  applyZone();
  esp_sntp_set_time_sync_notification_cb(onSync);
  DeviceLog::printf("[clock] server=%s timezone=%s stored=%d\n",
                gSettings.ntpServer, gSettings.timezone, gStored);
}

void requestSync() {
  if (esp_sntp_enabled()) esp_sntp_stop();
  gStarted = false;
  if (WiFi.status() != WL_CONNECTED) return;
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  int slot=0;
  for (char *server : {gSettings.ntpServer,gSettings.ntpServer2,gSettings.ntpServer3})
    if (server[0]) esp_sntp_setservername(slot++,server);
  while (slot<3) esp_sntp_setservername(slot++,nullptr);
  esp_sntp_set_sync_interval(kSyncIntervalMs);
  esp_sntp_init();
  gStarted = true;
}

void tick() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected && !gConnected) requestSync();
  gConnected = connected;
}

Snapshot snapshot() {
  Snapshot s;
  s.connected = WiFi.status() == WL_CONNECTED;
  s.lastSync = gLastSync.load();
  s.syncCount = gSyncCount.load();
  s.synced = s.lastSync != 0;
  const time_t now = time(nullptr);
  s.valid = now >= kEarliestTime;
  if (!s.valid) return s;
  s.unixTime = static_cast<uint32_t>(now);
  localtime_r(&now, &s.local);
  strftime(s.hhmm, sizeof(s.hhmm), "%H:%M", &s.local);
  strftime(s.seconds, sizeof(s.seconds), "%S", &s.local);
  strftime(s.weekday, sizeof(s.weekday), "%A", &s.local);
  strftime(s.date, sizeof(s.date), "%d %b %Y", &s.local);
  strftime(s.zone, sizeof(s.zone), "%Z", &s.local);
  strftime(s.iso, sizeof(s.iso), "%Y-%m-%dT%H:%M:%S%z", &s.local);
  return s;
}

String statusJson(bool includeZones) {
  const Snapshot s = snapshot();
  String json;
  json.reserve(includeZones ? 1600 : 500);
  json += "{\"ntp_server\":\"" + String(gSettings.ntpServer);
  json += "\",\"ntp_servers\":[\""+String(gSettings.ntpServer)+"\",\""+gSettings.ntpServer2+"\",\""+gSettings.ntpServer3+"\"]";
  json += ",\"timezone\":\"" + String(gSettings.timezone);
  json += "\",\"stored\":" + String(gStored ? "true" : "false");
  json += ",\"time_valid\":" + String(s.valid ? "true" : "false");
  json += ",\"synced\":" + String(s.synced ? "true" : "false");
  json += ",\"sntp_started\":" + String(gStarted ? "true" : "false");
  json += ",\"unix_time\":" + String(s.unixTime);
  json += ",\"last_sync_unix\":" + String(s.lastSync);
  json += ",\"sync_count\":" + String(s.syncCount);
  json += ",\"sync_interval_seconds\":3600,\"local_time\":\"" + String(s.iso);
  json += "\",\"zone_abbreviation\":\"" + String(s.zone) + "\"";
  if (includeZones) {
    json += ",\"zones\":[";
    bool first = true;
    for (const auto &zone : ClockSettings::kZones) {
      if (!first) json += ',';
      first = false;
      json += "{\"id\":\"" + String(zone.id) + "\",\"label\":\"" + zone.label + "\"}";
    }
    json += ']';
  }
  return json + '}';
}

bool configure(WebServer &server, String &error) {
  if (!server.hasArg("ntp_server") && !server.hasArg("ntp_server2") && !server.hasArg("ntp_server3") && !server.hasArg("timezone")) {
    error = "NTP server or timezone required"; return false;
  }
  ClockSettings::Stored candidate = gSettings;
  const char *fields[]={"ntp_server","ntp_server2","ntp_server3"};
  char *buffers[]={candidate.ntpServer,candidate.ntpServer2,candidate.ntpServer3};
  for (int i=0;i<3;++i) if (server.hasArg(fields[i])) {
    String name=server.arg(fields[i]); name.trim();
    if ((!i&&name.isEmpty())||(!name.isEmpty()&&!ClockSettings::validServer(name.c_str()))) {
      error="NTP servers must be hostnames or IPv4 addresses; the primary server is required"; return false;
    }
    name.toCharArray(buffers[i],254);
  }
  if (server.hasArg("timezone")) {
    const String zone = server.arg("timezone");
    if (!ClockSettings::findZone(zone.c_str())) { error = "unsupported timezone"; return false; }
    zone.toCharArray(candidate.timezone, sizeof(candidate.timezone));
  }
  if (!gPrefsOpen || gPrefs.putBytes("settings", &candidate, sizeof(candidate)) != sizeof(candidate)) {
    error = "could not save clock settings to NVS"; return false;
  }
  // Stop before replacing the long-lived hostname buffer borrowed by SNTP.
  if (esp_sntp_enabled()) esp_sntp_stop();
  const bool changedServer = std::strcmp(candidate.ntpServer, gSettings.ntpServer) != 0 ||
    std::strcmp(candidate.ntpServer2,gSettings.ntpServer2)!=0 || std::strcmp(candidate.ntpServer3,gSettings.ntpServer3)!=0;
  gSettings = candidate;
  gStored = true;
  if (changedServer) gLastSync.store(0);
  applyZone();
  requestSync();
  return true;
}
}  // namespace ClockService
