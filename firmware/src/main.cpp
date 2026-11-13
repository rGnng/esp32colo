// ColoPLAY (ESP32-S3-WROOM-1-N16R8) first-stage bring-up firmware.
//
// What it does:
//   * stores WiFi credentials in NVS (seeded from include/secrets.h on the
//     first boot), then joins that network
//   * serves a small status page at http://coloplay.local/ and /api/status
//   * lets you change the stored WiFi credentials at POST /api/wifi
//   * exposes the documented buttons (GPIO0 / GPIO15 / GPIO46) for testing
//   * enables ArduinoOTA for subsequent wireless updates
//
// It intentionally does NOT initialise the display, audio codec, or any other
// unverified peripheral.  That keeps this image safe to use as a first flash
// on a board whose exact variant is still unknown.

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <ESP.h>
#include <Preferences.h>

#include "secrets.h"

namespace {

constexpr const char *kHostname = "coloplay";
constexpr const char *kFirmwareVersion = "0.1.3-nvs";
constexpr const char *kPrefsNamespace = "wifi";
constexpr uint32_t kWifiRetryMs = 10000;

WebServer server(80);
Preferences wifiPrefs;
String gSsid;
String gPassword;

struct Button {
  const char *name;
  uint8_t pin;
  bool pullup;
};

// Pin assignments from the ColoPLAY V1 schematic (unverified on this unit):
//   REC_KEY -> GPIO0, OPEN_KEY -> GPIO15, knob push -> GPIO46.
constexpr Button kButtons[] = {
    {"REC_KEY", 0, true},
    {"OPEN_KEY", 15, true},
    {"KNOB", 46, false},  // input-only pin, external pull-up on the board
};

bool gOtaReady = false;
bool gPrefsOpen = false;
uint32_t gLastWifiAttempt = 0;
String gSerialLine;

String jsonEscape(const String &input) {
  String out;
  out.reserve(input.length() + 8);
  for (char c : input) {
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c; break;
    }
  }
  return out;
}

String htmlEscape(const String &input) {
  String out;
  out.reserve(input.length() + 8);
  for (char c : input) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out += c; break;
    }
  }
  return out;
}

// NVS is the source of truth.  On a blank device, seed it once from
// include/secrets.h so the first boot still joins the expected network.
void loadWifiCredentials() {
  gPrefsOpen = wifiPrefs.begin(kPrefsNamespace, false);
  if (gPrefsOpen) {
    gSsid = wifiPrefs.getString("ssid", "");
    gPassword = wifiPrefs.getString("pass", "");
  }

  if (gSsid.isEmpty()) {
    gSsid = WIFI_SSID;
    gPassword = WIFI_PASSWORD;
    if (gPrefsOpen) {
      bool saved = wifiPrefs.putString("ssid", gSsid) > 0;
      saved = (wifiPrefs.putString("pass", gPassword) > 0) && saved;
      Serial.println(saved ? "[wifi] seeded NVS credentials from secrets.h"
                           : "[wifi] failed to seed NVS; using secrets.h");
    } else {
      Serial.println("[wifi] NVS open failed; using secrets.h without saving");
    }
  } else {
    Serial.printf("[wifi] loaded credentials from NVS (ssid=%s)\n", gSsid.c_str());
  }
}

bool saveWifiCredentials(const String &ssid, const String &password) {
  if (ssid.isEmpty() || !gPrefsOpen) {
    return false;
  }
  bool ok = wifiPrefs.putString("ssid", ssid) > 0;
  ok = (wifiPrefs.putString("pass", password) > 0) && ok;
  if (ok) {
    gSsid = ssid;
    gPassword = password;
  }
  return ok;
}

void connectWifi() {
  gLastWifiAttempt = millis();

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.printf("[wifi] connecting to \"%s\"...\n", gSsid.c_str());
  WiFi.disconnect();
  WiFi.begin(gSsid.c_str(), gPassword.c_str());
}

void logWifiResult() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] connected: ip=%s rssi=%d dBm mac=%s\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI(),
                  WiFi.macAddress().c_str());
  } else {
    Serial.printf("[wifi] not connected (status=%d), retrying\n", WiFi.status());
  }
}

String buttonStateJson() {
  String json = "[";
  for (size_t i = 0; i < sizeof(kButtons) / sizeof(kButtons[0]); ++i) {
    if (i) json += ",";
    json += "{\"name\":\"";
    json += kButtons[i].name;
    json += "\",\"pin\":";
    json += String(kButtons[i].pin);
    json += ",\"pressed\":";
    json += digitalRead(kButtons[i].pin) == LOW ? "true" : "false";
    json += "}";
  }
  json += "]";
  return json;
}

String statusJson() {
  String json;
  json.reserve(512);
  json += "{";
  json += "\"firmware\":\"" + String(kFirmwareVersion) + "\",";
  json += "\"hostname\":\"" + String(kHostname) + "\",";
  json += "\"chip\":\"" + String(ESP.getChipModel()) + "\",";
  json += "\"chip_revision\":" + String(ESP.getChipRevision()) + ",";
  json += "\"flash_bytes\":" + String(ESP.getFlashChipSize()) + ",";
  json += "\"psram_bytes\":" + String(ESP.getPsramSize()) + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"uptime_ms\":" + String(millis()) + ",";
  json += "\"wifi\":{";
  json += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"ssid\":\"" + jsonEscape(WiFi.SSID()) + "\",";
  json += "\"stored_ssid\":\"" + jsonEscape(gSsid) + "\",";
  json += "\"source\":\"nvs\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"mac\":\"" + WiFi.macAddress() + "\"";
  json += "},";
  json += "\"buttons\":" + buttonStateJson();
  json += "}";
  return json;
}

// Serial recovery console on the USB-CDC port.  This works even when the
// stored WiFi credentials are wrong and the web interface is unreachable.
void handleSerialCommand(const String &line) {
  if (line == "status") {
    Serial.println(statusJson());
    return;
  }
  if (line == "wifi-reset") {
    if (gPrefsOpen) {
      wifiPrefs.clear();
    }
    Serial.println("[wifi] cleared NVS wifi namespace; rebooting");
    delay(200);
    ESP.restart();
    return;
  }
  if (line.startsWith("wifi ")) {
    String rest = line.substring(5);
    rest.trim();
    int split = rest.indexOf(' ');
    String ssid = split < 0 ? rest : rest.substring(0, split);
    String password = split < 0 ? "" : rest.substring(split + 1);
    ssid.trim();
    if (password.isEmpty()) {
      password = gPassword;
    }
    if (saveWifiCredentials(ssid, password)) {
      Serial.printf("[wifi] saved %s to NVS; rebooting\n", ssid.c_str());
      delay(200);
      ESP.restart();
    } else {
      Serial.println("[wifi] save failed");
    }
    return;
  }
  Serial.println("[serial] commands: status | wifi <ssid> <password> | wifi-reset");
}

void pollSerial() {
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      if (!gSerialLine.isEmpty()) {
        handleSerialCommand(gSerialLine);
        gSerialLine = "";
      }
    } else if (gSerialLine.length() < 128) {
      gSerialLine += c;
    }
  }
}

String htmlPage() {
  String ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "not connected";
  String page;
  page.reserve(2048);
  page += F(
      "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      "<meta http-equiv=\"refresh\" content=\"3\">"
      "<title>ColoPLAY bring-up</title>"
      "<style>"
      "body{font:15px/1.5 system-ui,sans-serif;max-width:640px;margin:40px auto;padding:0 16px;color:#19332e}"
      "h1{font-size:22px}table{border-collapse:collapse;width:100%}"
      "td{padding:6px 8px;border-bottom:1px solid #d6dfd5}td:first-child{color:#53665e;width:160px}"
      "code{font-size:13px}.on{color:#096b54;font-weight:600}.off{color:#865000}"
      "</style></head><body>");
  page += F("<h1>ColoPLAY bring-up firmware</h1><table>");
  page += "<tr><td>Firmware</td><td><code>" + String(kFirmwareVersion) + "</code></td></tr>";
  page += "<tr><td>Chip</td><td>" + String(ESP.getChipModel()) + " rev " +
          String(ESP.getChipRevision()) + "</td></tr>";
  page += "<tr><td>Flash</td><td>" + String(ESP.getFlashChipSize() / 1024 / 1024) + " MB</td></tr>";
  page += "<tr><td>PSRAM</td><td>" + String(ESP.getPsramSize() / 1024 / 1024) + " MB</td></tr>";
  page += "<tr><td>Free heap</td><td>" + String(ESP.getFreeHeap() / 1024) + " KB</td></tr>";
  page += "<tr><td>Uptime</td><td>" + String(millis() / 1000) + " s</td></tr>";
  page += "<tr><td>WiFi SSID</td><td>" + htmlEscape(gSsid) + " (NVS)</td></tr>";
  page += "<tr><td>IP address</td><td>" + ip + "</td></tr>";
  page += "<tr><td>RSSI</td><td>" + String(WiFi.RSSI()) + " dBm</td></tr>";
  page += "<tr><td>MAC</td><td><code>" + WiFi.macAddress() + "</code></td></tr>";

  for (const auto &button : kButtons) {
    bool pressed = digitalRead(button.pin) == LOW;
    page += "<tr><td>" + String(button.name) + " (GPIO" + String(button.pin) + ")</td><td>";
    page += pressed ? F("<span class=\"on\">pressed</span>")
                    : F("<span class=\"off\">released</span>");
    page += "</td></tr>";
  }

  page += F("</table><h2>WiFi credentials (NVS)</h2>"
            "<p>Saving restarts the device. Leave the password empty to keep the current one.</p>"
            "<form method=\"post\" action=\"/api/wifi\">"
            "<label>SSID <input name=\"ssid\" required value=\"");
  page += htmlEscape(gSsid);
  page += F("\"></label><br><br>"
            "<label>Password <input name=\"password\" type=\"password\"></label><br><br>"
            "<button type=\"submit\">Save and reboot</button>"
            "</form>"
            "<p><a href=\"/api/status\">JSON status</a></p>"
            "<p>Display, audio and input handling are not implemented yet.</p>"
            "</body></html>");
  return page;
}

void setupHttp() {
  server.on("/", HTTP_GET, [] { server.send(200, "text/html; charset=utf-8", htmlPage()); });
  server.on("/api/status", HTTP_GET, [] {
    server.send(200, "application/json", statusJson());
  });
  server.on("/api/wifi", HTTP_GET, [] {
    String json = "{\"ssid\":\"" + jsonEscape(gSsid) + "\",\"source\":\"nvs\"}";
    server.send(200, "application/json", json);
  });
  server.on("/api/wifi", HTTP_POST, [] {
    if (!server.hasArg("ssid")) {
      server.send(400, "application/json", "{\"error\":\"ssid required\"}");
      return;
    }
    String ssid = server.arg("ssid");
    ssid.trim();
    String password = server.arg("password");
    if (password.isEmpty()) {
      password = gPassword;
    }
    if (ssid.isEmpty() || !saveWifiCredentials(ssid, password)) {
      server.send(500, "application/json", "{\"error\":\"could not save to NVS\"}");
      return;
    }
    Serial.printf("[wifi] saved credentials to NVS (ssid=%s), rebooting\n", ssid.c_str());
    server.send(200, "application/json", "{\"saved\":true,\"rebooting\":true}");
    delay(300);
    ESP.restart();
  });
  server.on("/reboot", HTTP_POST, [] {
    server.send(200, "text/plain", "rebooting\n");
    delay(200);
    ESP.restart();
  });
  server.onNotFound([] {
    server.send(404, "text/plain", "not found\n");
  });
  server.begin();
  Serial.println("[http] server listening on port 80");
}

void setupOta() {
  ArduinoOTA.setHostname(kHostname);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([] {
    const String type = ArduinoOTA.getCommand() == U_FLASH ? "firmware" : "filesystem";
    Serial.printf("[ota] update started (%s)\n", type.c_str());
  });
  ArduinoOTA.onEnd([] { Serial.println("\n[ota] update finished"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[ota] %u%%\r", total ? (progress * 100) / total : 0);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[ota] error %u\n", static_cast<unsigned>(error));
  });

  ArduinoOTA.begin();
  gOtaReady = true;
  Serial.println("[ota] ArduinoOTA ready");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 3000) {
    delay(10);
  }

  Serial.println();
  Serial.printf("ColoPLAY bring-up firmware %s\n", kFirmwareVersion);
  Serial.println("[serial] commands: status | wifi <ssid> <password> | wifi-reset");
  Serial.printf("[chip] %s rev %d, flash %u MB, psram %u MB\n",
                ESP.getChipModel(), ESP.getChipRevision(),
                ESP.getFlashChipSize() / 1024 / 1024,
                ESP.getPsramSize() / 1024 / 1024);

  for (const auto &button : kButtons) {
    pinMode(button.pin, button.pullup ? INPUT_PULLUP : INPUT);
  }

  loadWifiCredentials();

  // The application owns the credentials in the "wifi" NVS namespace.
  // Stop the WiFi driver from keeping a second copy in its own namespace.
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(kHostname);

  connectWifi();
  const uint32_t connectStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - connectStart < 20000) {
    delay(250);
  }
  logWifiResult();

  if (WiFi.status() == WL_CONNECTED) {
    if (MDNS.begin(kHostname)) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("[mdns] http://%s.local/\n", kHostname);
    } else {
      Serial.println("[mdns] failed to start");
    }
    setupOta();
  } else {
    Serial.println("[wifi] initial connection failed; OTA disabled until connected");
  }

  setupHttp();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - gLastWifiAttempt >= kWifiRetryMs) {
      connectWifi();
    }
  } else if (!gOtaReady) {
    // First successful (re)connection after boot: bring mDNS and OTA up.
    if (MDNS.begin(kHostname)) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("[mdns] http://%s.local/\n", kHostname);
    }
    setupOta();
  }

  if (WiFi.status() == WL_CONNECTED && gOtaReady) {
    ArduinoOTA.handle();
  }
  server.handleClient();
  pollSerial();
  delay(2);
}
