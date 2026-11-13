// ColoPLAY V1 hardware demo. Keep the factory bootloader / partition tables.

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Esp.h>
#include <Preferences.h>
#include <esp_ota_ops.h>

#include "secrets.h"
#include "board_pins.h"
#include "hardware_demo.h"
#include "http_ota.h"
#include "web_ui.h"

namespace {

constexpr const char *kHostname = "coloplay";
constexpr const char *kFirmwareVersion = "0.2.5-rotary";
constexpr const char *kPrefsNamespace = "wifi";
constexpr uint32_t kWifiRetryMs = 10000;

WebServer server(80);
Preferences wifiPrefs;
Preferences demoPrefs;
String gSsid;
String gPassword;

struct Button {
  const char *name;
  uint8_t pin;
  bool pullup;
};

// Early inputs for recovery and telemetry before the demo starts.
constexpr Button kButtons[] = {
    {"KEY1", Board::kKey1, true},
    {"KEY2", Board::kKey2, true},
    {"KEY3", Board::kKey3, true},
    {"KEY4", Board::kKey4, true},
    {"REC_KEY", 0, true},
    {"OPEN_KEY", 15, true},
    {"KNOB", 46, false},  // input-only pin, external pull-up on the board
};

bool gOtaReady = false;
bool gPrefsOpen = false;
bool gDemoPrefsOpen = false, gRecoveryMode = false, gHardwareStarted = false;
bool gBootConfirmed = false, gArduinoOtaBusy = false;
uint8_t gBootAttempts = 0;
uint32_t gHardwareStartAt = 0, gHardwareStartedAt = 0;
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
  if (HardwareDemo::ready()) return HardwareDemo::buttonsJson();
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
  json.reserve(2400);
  json += "{";
  json += "\"firmware\":\"" + String(kFirmwareVersion) + "\",";
  json += "\"sketch_md5\":\"" + ESP.getSketchMD5() + "\",";
  json += "\"hostname\":\"" + String(kHostname) + "\",";
  json += "\"chip\":\"" + String(ESP.getChipModel()) + "\",";
  json += "\"chip_revision\":" + String(ESP.getChipRevision()) + ",";
  json += "\"flash_bytes\":" + String(ESP.getFlashChipSize()) + ",";
  json += "\"psram_bytes\":" + String(ESP.getPsramSize()) + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"uptime_ms\":" + String(millis()) + ",";
  json += "\"reset_reason\":" + String(static_cast<int>(esp_reset_reason())) + ",";
  json += "\"recovery_mode\":" + String(gRecoveryMode ? "true" : "false") + ",";
  json += "\"boot_attempts\":" + String(gBootAttempts) + ",";
  json += "\"http_ota\":true,";
  const esp_partition_t *running = esp_ota_get_running_partition();
  json += "\"running_partition\":\"" + String(running ? running->label : "unknown") + "\",";
  json += "\"wifi\":{";
  json += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"ssid\":\"" + jsonEscape(WiFi.SSID()) + "\",";
  json += "\"stored_ssid\":\"" + jsonEscape(gSsid) + "\",";
  json += "\"source\":\"nvs\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"mac\":\"" + WiFi.macAddress() + "\"";
  json += "},";
  json += "\"buttons\":" + buttonStateJson() + ",";
  json += "\"hardware\":" + HardwareDemo::statusJson();
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

void setupHttp() {
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html; charset=utf-8", kDemoPage); });
  server.on("/api/status", HTTP_GET, [] {
    server.send(200, "application/json", statusJson());
  });
  server.on("/api/wifi", HTTP_GET, [] {
    String json = "{\"ssid\":\"" + jsonEscape(gSsid) + "\",\"source\":\"nvs\"}";
    server.send(200, "application/json", json);
  });
  server.on("/api/demo", HTTP_POST, [] {
    String error;
    if (!HardwareDemo::configure(server, error)) {
      server.send(400, "application/json", "{\"error\":\"" + jsonEscape(error) + "\"}");
      return;
    }
    server.send(200, "application/json", HardwareDemo::statusJson());
  });
  server.on("/api/screenshot.bmp", HTTP_GET, [] { HardwareDemo::sendScreenshot(server); });
  server.on("/api/hardware/retry", HTTP_POST, [] {
    if (!server.authenticate("admin", OTA_PASSWORD)) {
      server.requestAuthentication(BASIC_AUTH, "ColoPLAY OTA");
      return;
    }
    if (gDemoPrefsOpen) demoPrefs.putUChar("attempts", 0);
    server.send(200, "application/json", "{\"retrying\":true,\"rebooting\":true}");
    delay(200);
    ESP.restart();
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
    if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 63) {
      server.send(400, "application/json", "{\"error\":\"invalid WiFi credentials length\"}");
      return;
    }
    if (!saveWifiCredentials(ssid, password)) {
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
  HttpOta::setup(server, OTA_PASSWORD);
  server.begin();
  Serial.println("[http] server listening on port 80");
}

void setupOta() {
  ArduinoOTA.setHostname(kHostname);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([] {
    gArduinoOtaBusy = true;
    HardwareDemo::pause(true);
    const String type = ArduinoOTA.getCommand() == U_FLASH ? "firmware" : "filesystem";
    Serial.printf("[ota] update started (%s)\n", type.c_str());
  });
  ArduinoOTA.onEnd([] { Serial.println("\n[ota] update finished"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[ota] %u%%\r", total ? (progress * 100) / total : 0);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    gArduinoOtaBusy = false;
    HardwareDemo::pause(false);
    Serial.printf("[ota] error %u\n", static_cast<unsigned>(error));
  });

  ArduinoOTA.begin();
  gOtaReady = true;
  Serial.println("[ota] ArduinoOTA ready");
}

}  // namespace

void setup() {
  HardwareDemo::captureBootStage();
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
  gDemoPrefsOpen = demoPrefs.begin("demo", false);
  if (gDemoPrefsOpen) {
    gBootAttempts = demoPrefs.getUChar("attempts", 0);
    // An updated image gets its own retry budget. Keep the guard across
    // resets of the same image, including resets after a peripheral panic.
    const String image = ESP.getSketchMD5();
    if (image != demoPrefs.getString("image", "") &&
        demoPrefs.putString("image", image) > 0) {
      gBootAttempts = 0;
      demoPrefs.putUChar("attempts", 0);
    }
  }
  // Two failed starts leave the web server and OTA available on the third
  // boot. Holding KEY1 during reset also skips peripheral setup. KEY1 is
  // not a boot strap; REC (GPIO0) is reserved for ROM download mode.
  gRecoveryMode = gBootAttempts >= 2 || digitalRead(Board::kKey1) == LOW;

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
  // Give both update methods a recovery window before touching peripherals.
  gHardwareStartAt = millis() + 5000;
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

  if (WiFi.status() == WL_CONNECTED && gOtaReady && !HttpOta::busy()) {
    ArduinoOTA.handle();
  }
  server.handleClient();
  HttpOta::tick();
  pollSerial();
  if (!gArduinoOtaBusy && !HttpOta::busy()) {
    if (!gHardwareStarted && !gRecoveryMode &&
        static_cast<int32_t>(millis() - gHardwareStartAt) >= 0) {
      gHardwareStarted = true;
      ++gBootAttempts;
      if (gDemoPrefsOpen) demoPrefs.putUChar("attempts", gBootAttempts);
      HardwareDemo::begin();
      gHardwareStartedAt = millis();
    }
    HardwareDemo::tick();
    if (HardwareDemo::ready() && !gBootConfirmed &&
        millis() - gHardwareStartedAt >= 30000) {
      gBootConfirmed = true;
      gBootAttempts = 0;
      if (gDemoPrefsOpen) demoPrefs.putUChar("attempts", 0);
    }
  }
  delay(2);
}
