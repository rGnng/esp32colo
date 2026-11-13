#include "http_ota.h"
#include "hardware_demo.h"
#include "web_ui.h"
#include "app_image.h"
#include <Update.h>
#include <esp_ota_ops.h>
#include <algorithm>

namespace HttpOta {
namespace {
const char *gPassword = nullptr;
bool gBusy = false, gAuthorized = false, gSawFile = false, gFinished = false;
uint32_t gRestartAt = 0, gBytes = 0, gLastActivity = 0;
uint8_t gHeader[AppImage::kPrefixBytes];
size_t gHeaderBytes = 0;
String gError;

void fail(const String &message) {
  gError = message;
  if (Update.isRunning()) Update.abort();
  gBusy = false;
  HardwareDemo::pause(false);
  Serial.printf("[http-ota] %s\n", message.c_str());
}

bool authorize(WebServer &server) {
  if (!gPassword || !*gPassword) {
    server.send(503, "application/json", "{\"error\":\"OTA password is not configured\"}");
    return false;
  }
  if (server.authenticate("admin", gPassword)) return true;
  server.requestAuthentication(BASIC_AUTH, "ColoPLAY OTA");
  return false;
}
}  // namespace

void setup(WebServer &server, const char *password) {
  gPassword = password;
  const char *headers[] = {"X-MD5"};
  server.collectHeaders(headers, 1);
  server.on("/update", HTTP_GET, [&server] {
    if (authorize(server)) server.send_P(200, "text/html; charset=utf-8", kUpdatePage);
  });
  server.on("/update", HTTP_POST, [&server] {
    if (!authorize(server)) {
      if (Update.isRunning()) fail("request authentication failed");
      gSawFile = false;
      return;
    }
    bool ok = gSawFile && gFinished && gError.isEmpty();
    if (ok && !Update.end(true)) { fail(Update.errorString()); ok = false; }
    gSawFile = false;
    if (ok) {
      Serial.printf("[http-ota] verified %u bytes; application selected for next boot\n", gBytes);
      server.send(200, "application/json", "{\"updated\":true,\"rebooting\":true}");
      gRestartAt = millis() + 1000;
    } else {
      if (gError.isEmpty()) fail("no complete firmware file received");
      server.send(400, "application/json", "{\"error\":\"firmware rejected; see serial log\"}");
    }
  }, [&server] {
    HTTPUpload &upload = server.upload();
    gLastActivity = millis();
    if (upload.status == UPLOAD_FILE_START) {
      if (gRestartAt) return;
      if (gSawFile && gAuthorized) return fail("only one firmware file is accepted");
      // Reject before touching flash. The finish handler supplies the 401.
      gAuthorized = gPassword && *gPassword && server.authenticate("admin", gPassword);
      gSawFile = true;
      gFinished = false;
      gHeaderBytes = gBytes = 0;
      gError = "";
      if (!gAuthorized) return;
      Serial.println("[http-ota] receiving application image");
    }
    if (gRestartAt || !gAuthorized || !gError.isEmpty()) return;
    if (upload.status == UPLOAD_FILE_WRITE) {
      size_t consumed = 0;
      if (gHeaderBytes < sizeof(gHeader)) {
        const size_t count = std::min(sizeof(gHeader) - gHeaderBytes, upload.currentSize);
        memcpy(gHeader + gHeaderBytes, upload.buf, count);
        gHeaderBytes += count;
        consumed = count;
        if (gHeaderBytes < sizeof(gHeader)) return;
        // Check both chip ID and application descriptor before erasing.
        // A merged image / bootloader is not an application OTA payload.
        if (!AppImage::validPrefix(gHeader, sizeof(gHeader))) {
          return fail("expected an ESP32-S3 application image");
        }
        const esp_partition_t *target = esp_ota_get_next_update_partition(nullptr);
        if (!target || target->size != 0x480000) return fail("unexpected OTA partition layout");
        HardwareDemo::pause(true);
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) return fail(Update.errorString());
        const String md5 = server.header("X-MD5");
        if (md5.length() && !Update.setMD5(md5.c_str())) return fail("invalid X-MD5 header");
        gBusy = true;
        if (Update.write(gHeader, sizeof(gHeader)) != sizeof(gHeader)) return fail(Update.errorString());
        gBytes = sizeof(gHeader);
      }
      const size_t count = upload.currentSize - consumed;
      if (count && Update.write(upload.buf + consumed, count) != count) return fail(Update.errorString());
      gBytes += count;
    } else if (upload.status == UPLOAD_FILE_END) {
      if (gHeaderBytes != sizeof(gHeader) || !Update.isRunning() || gBytes != upload.totalSize) {
        return fail("incomplete firmware image");
      }
      gFinished = true;
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
      fail("upload aborted; previous application remains selected");
    }
  });
}

bool busy() { return gBusy; }

void tick() {
  // A disconnected client may vanish before WebServer delivers the POST
  // completion callback. Release flash / peripherals and allow a retry.
  if (gSawFile && gAuthorized && !gRestartAt && millis() - gLastActivity > 15000) {
    fail("upload timed out; running application retained");
    gSawFile = false;
  }
  if (gRestartAt && static_cast<int32_t>(millis() - gRestartAt) >= 0) ESP.restart();
}
}  // namespace HttpOta
